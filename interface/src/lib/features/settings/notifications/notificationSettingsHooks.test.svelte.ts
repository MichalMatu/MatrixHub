import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockHeartbeatApi, mockNotificationApi, mockNotifications } = vi.hoisted(() => ({
	mockNotificationApi: {
		getSettings: vi.fn(),
		updateSettings: vi.fn()
	},
	mockHeartbeatApi: {
		getSettings: vi.fn(),
		updateSettings: vi.fn()
	},
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn(),
		warning: vi.fn(),
		info: vi.fn()
	}
}));

vi.mock('$lib/services/api/integrations/NotificationApiService', () => ({
	notificationApi: mockNotificationApi
}));

vi.mock('$lib/services/api/integrations/HeartbeatApiService', () => ({
	heartbeatApi: mockHeartbeatApi
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown';
	}),
	isValidTelegramBotToken: vi.fn((value: string) => /^\d{3,}:[A-Za-z0-9_-]{10,}$/.test(value)),
	isValidHttpUrl: vi.fn((value: string) => {
		try {
			const url = new URL(value);
			return url.protocol === 'http:' || url.protocol === 'https:';
		} catch {
			return false;
		}
	})
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	settings_load_error: () => 'load error',
	settings_save_error: () => 'save error',
	settings_saved: () => 'saved',
	settings_validation_error: () => 'validation error',
	toast_message: ({ message }: { message: string }) => message,
	toast_heartbeat_url_required: () => 'heartbeat url required'
}));

function createNotificationSettings(
	overrides: Partial<{
		telegram_enabled: boolean;
		webhook_enabled: boolean;
		bot_token: string;
		chat_id: string;
		commands_enabled: boolean;
		webhook_url: string;
		pushover_enabled: boolean;
		pushover_user: string;
		pushover_token: string;
		is_configured: boolean;
	}> = {}
) {
	return {
		telegram_enabled: false,
		webhook_enabled: false,
		bot_token: '',
		chat_id: '',
		commands_enabled: false,
		webhook_url: '',
		pushover_enabled: false,
		pushover_user: '',
		pushover_token: '',
		is_configured: false,
		...overrides
	};
}

function createHeartbeatSettings(
	overrides: Partial<{
		interval_ms: number;
		slots: Array<{ enabled: boolean; name: string; url: string; allow_insecure: boolean }>;
	}> = {}
) {
	return {
		interval_ms: 300000,
		slots: [],
		...overrides
	};
}

function flushPromises() {
	return new Promise((resolve) => setTimeout(resolve, 0));
}

describe('notification settings hooks', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockNotificationApi.getSettings.mockResolvedValue(createNotificationSettings());
		mockNotificationApi.updateSettings.mockResolvedValue(createNotificationSettings());
		mockHeartbeatApi.getSettings.mockResolvedValue(createHeartbeatSettings());
		mockHeartbeatApi.updateSettings.mockResolvedValue(createHeartbeatSettings());
	});

	it('telegram sanitizes ASCII input and blocks invalid token saves', async () => {
		const { useTelegramSettings } = await import('./telegram/useTelegramSettings.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const telegram = useTelegramSettings({
					api: mockNotificationApi as never
				});

				void telegram.loadSettings().then(() => {
					telegram.updateSetting('enabled', true);
					telegram.updateSetting('bot_token', 'bad\u0007token');
					telegram.updateSetting('chat_id', '12345\u0000');
					telegram.saveSettings();

					expect(telegram.settings.bot_token).toBe('badtoken');
					expect(telegram.settings.chat_id).toBe('12345');
					expect(mockNotificationApi.updateSettings).not.toHaveBeenCalled();
					expect(telegram.errors.bot_token).toBe(true);
					expect(mockNotifications.warning).toHaveBeenCalledWith('validation error', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('telegram auto-loads settings once per access cycle', async () => {
		const { useTelegramSettings } = await import('./telegram/useTelegramSettings.svelte');

		let cleanup: (() => void) | undefined;
		let setCanLoad!: (value: boolean) => void;

		cleanup = $effect.root(() => {
			let canLoad = $state(false);
			setCanLoad = (value: boolean) => {
				canLoad = value;
			};

			useTelegramSettings({
				api: mockNotificationApi as never,
				shouldLoad: () => canLoad
			});
		});

		expect(mockNotificationApi.getSettings).not.toHaveBeenCalled();

		setCanLoad(true);
		await vi.waitFor(() => {
			expect(mockNotificationApi.getSettings).toHaveBeenCalledTimes(1);
		});

		await flushPromises();
		expect(mockNotificationApi.getSettings).toHaveBeenCalledTimes(1);

		setCanLoad(false);
		await flushPromises();
		setCanLoad(true);

		await vi.waitFor(() => {
			expect(mockNotificationApi.getSettings).toHaveBeenCalledTimes(2);
		});

		cleanup?.();
	});

	it('webhook blocks invalid enabled URLs', async () => {
		const { useWebhookSettings } = await import('./webhook/useWebhookSettings.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const webhook = useWebhookSettings({
					api: mockNotificationApi as never
				});

				void webhook.loadSettings().then(() => {
					webhook.updateSetting('enabled', true);
					webhook.updateSetting('url', 'ftp://bad-url\u0007');
					webhook.saveSettings();

					expect(webhook.settings.url).toBe('ftp://bad-url');
					expect(mockNotificationApi.updateSettings).not.toHaveBeenCalled();
					expect(webhook.errors.url).toBe(true);
					expect(mockNotifications.warning).toHaveBeenCalledWith('validation error', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('pushover saves sanitized credentials and preserves is_configured payload', async () => {
		const { usePushoverSettings } = await import('./pushover/usePushoverSettings.svelte');

		mockNotificationApi.updateSettings.mockResolvedValue(
			createNotificationSettings({
				pushover_enabled: true,
				pushover_user: 'user-key',
				pushover_token: 'app-token',
				is_configured: true
			})
		);

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const pushover = usePushoverSettings({
					api: mockNotificationApi as never
				});

				void pushover.loadSettings().then(async () => {
					pushover.updateSetting('pushover_enabled', true);
					pushover.updateSetting('pushover_user', 'user-key\u0007');
					pushover.updateSetting('pushover_token', 'app-token\u0000');
					pushover.saveSettings();

					await flushPromises();

					expect(mockNotificationApi.updateSettings).toHaveBeenCalledWith({
						pushover_enabled: true,
						pushover_user: 'user-key',
						pushover_token: 'app-token',
						is_configured: true
					});
					expect(pushover.settings.pushover_user).toBe('user-key');
					expect(pushover.settings.pushover_token).toBe('app-token');
					expect(pushover.hasChanges).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('heartbeat blocks enabled slots without URL', async () => {
		const { useHeartbeatSettings } = await import('./heartbeat/useHeartbeatSettings.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const heartbeat = useHeartbeatSettings({
					api: mockHeartbeatApi as never
				});

				void heartbeat.loadSettings().then(() => {
					heartbeat.toggleSlot(0);
					heartbeat.saveSettings();

					expect(mockHeartbeatApi.updateSettings).not.toHaveBeenCalled();
					expect(mockNotifications.warning).toHaveBeenCalledWith('heartbeat url required', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('heartbeat preserves https URLs and forwards allow_insecure', async () => {
		const { useHeartbeatSettings } = await import('./heartbeat/useHeartbeatSettings.svelte');

		mockHeartbeatApi.updateSettings.mockResolvedValue(
			createHeartbeatSettings({
				slots: [
					{
						enabled: true,
						name: 'Primary',
						url: 'https://example.com/ping',
						allow_insecure: true
					}
				]
			})
		);

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const heartbeat = useHeartbeatSettings({
					api: mockHeartbeatApi as never
				});

				void heartbeat.loadSettings().then(async () => {
					heartbeat.toggleSlot(0);
					heartbeat.setSlotName(0, 'Primary');
					heartbeat.setSlotUrl(0, 'https://example.com/ping');
					heartbeat.setSlotAllowInsecure(0, true);
					heartbeat.saveSettings();

					await flushPromises();

					expect(mockHeartbeatApi.updateSettings).toHaveBeenCalledWith({
						interval_ms: 300000,
						slots: [
							{
								enabled: true,
								name: 'Primary',
								url: 'https://example.com/ping',
								allow_insecure: true
							},
							{ enabled: false, name: '', url: '', allow_insecure: false },
							{ enabled: false, name: '', url: '', allow_insecure: false },
							{ enabled: false, name: '', url: '', allow_insecure: false }
						]
					});
					expect(heartbeat.canTest).toBe(true);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('heartbeat keeps canTest true until a disabling save succeeds', async () => {
		const { useHeartbeatSettings } = await import('./heartbeat/useHeartbeatSettings.svelte');

		mockHeartbeatApi.getSettings.mockResolvedValue(
			createHeartbeatSettings({
				slots: [
					{
						enabled: true,
						name: 'Primary',
						url: 'https://example.com/ping',
						allow_insecure: false
					}
				]
			})
		);
		mockHeartbeatApi.updateSettings.mockResolvedValue(createHeartbeatSettings());

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const heartbeat = useHeartbeatSettings({
					api: mockHeartbeatApi as never
				});

				void heartbeat.loadSettings().then(async () => {
					expect(heartbeat.canTest).toBe(true);

					heartbeat.toggleSlot(0);
					expect(heartbeat.canTest).toBe(true);

					heartbeat.saveSettings();
					await flushPromises();

					expect(heartbeat.canTest).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
