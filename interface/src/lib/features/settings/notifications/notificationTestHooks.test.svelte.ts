import { beforeEach, describe, expect, it, vi } from 'vitest';

const { getRequestAbortKind, ApiError, mockNotificationApi, mockNotifications } = vi.hoisted(
	() => ({
		getRequestAbortKind: vi.fn<() => string | null>(() => null),
		ApiError: class ApiError extends Error {
			constructor(
				public status: number,
				message: string
			) {
				super(message);
				this.name = 'ApiError';
			}
		},
		mockNotificationApi: {
			testTelegram: vi.fn(),
			testWebhook: vi.fn(),
			testPushover: vi.fn()
		},
		mockNotifications: {
			success: vi.fn(),
			error: vi.fn(),
			warning: vi.fn(),
			info: vi.fn()
		}
	})
);

vi.mock('$lib/services/api/integrations/NotificationApiService', () => ({
	notificationApi: mockNotificationApi
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
	getRequestAbortKind,
	ApiError
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	telegram_test_default_message: () => 'Test Telegram message',
	toast_message: ({ message }: { message: string }) => message,
	toast_telegram_test_message_required: () => 'telegram message required',
	toast_telegram_test_sent: () => 'telegram sent',
	toast_telegram_not_configured: () => 'telegram not configured',
	toast_telegram_test_failed: () => 'telegram failed',
	toast_telegram_test_timeout: () => 'telegram timeout',
	toast_webhook_test_sent: () => 'webhook sent',
	toast_webhook_not_configured: () => 'webhook not configured',
	toast_webhook_test_failed: () => 'webhook failed',
	toast_webhook_test_timeout: () => 'webhook timeout',
	toast_test_in_progress: () => 'test in progress',
	pushover_test_success: () => 'pushover sent',
	error_prefix: ({ error }: { error: string }) => `error: ${error}`
}));

function flushPromises() {
	return new Promise((resolve) => setTimeout(resolve, 0));
}

describe('notification test hooks', () => {
	beforeEach(async () => {
		vi.clearAllMocks();
		getRequestAbortKind.mockReturnValue(null);
		mockNotificationApi.testTelegram.mockResolvedValue({ ok: true });
		mockNotificationApi.testWebhook.mockResolvedValue({ ok: true });
		mockNotificationApi.testPushover.mockResolvedValue({ ok: true });
		const { resetGlobalTestExecutionGateForTests } = await import('../testExecutionGate.svelte');
		resetGlobalTestExecutionGateForTests();
	});

	it('telegram trims text and prevents concurrent sends', async () => {
		const { useTelegramTest } = await import('./telegram/useTelegramTest.svelte');

		let resolveRequest: ((value: { ok: boolean }) => void) | undefined;
		mockNotificationApi.testTelegram.mockImplementation(
			() =>
				new Promise<{ ok: boolean }>((resolve) => {
					resolveRequest = resolve;
				})
		);

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const telegram = useTelegramTest({ api: mockNotificationApi as never });
				telegram.testText = '  hello  ';

				void telegram.sendTest();
				void telegram.sendTest();

				expect(mockNotificationApi.testTelegram).toHaveBeenCalledTimes(1);
				expect(mockNotificationApi.testTelegram).toHaveBeenCalledWith(
					'hello',
					expect.any(AbortSignal)
				);

				resolveRequest?.({ ok: true });

				void flushPromises().then(() => {
					expect(telegram.sending).toBe(false);
					expect(mockNotifications.success).toHaveBeenCalledWith('telegram sent', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('blocks a different test hook while another test request is in flight', async () => {
		const { useTelegramTest } = await import('./telegram/useTelegramTest.svelte');
		const { useWebhookTest } = await import('./webhook/useWebhookTest.svelte');

		let resolveRequest: ((value: { ok: boolean }) => void) | undefined;
		mockNotificationApi.testTelegram.mockImplementation(
			() =>
				new Promise<{ ok: boolean }>((resolve) => {
					resolveRequest = resolve;
				})
		);

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const telegram = useTelegramTest({ api: mockNotificationApi as never });
				const webhook = useWebhookTest(() => '', { api: mockNotificationApi as never });

				void telegram.sendTest();
				void webhook.sendTest();

				expect(mockNotificationApi.testTelegram).toHaveBeenCalledTimes(1);
				expect(mockNotificationApi.testWebhook).not.toHaveBeenCalled();

				resolveRequest?.({ ok: true });

				void flushPromises().then(() => {
					expect(telegram.sending).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('telegram maps aborts to timeout feedback', async () => {
		const { useTelegramTest } = await import('./telegram/useTelegramTest.svelte');

		getRequestAbortKind.mockReturnValue('timeout');
		mockNotificationApi.testTelegram.mockRejectedValue(new Error('aborted'));

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const telegram = useTelegramTest({ api: mockNotificationApi as never });

				void telegram.sendTest().then(() => {
					expect(telegram.lastError).toBe('telegram timeout');
					expect(mockNotifications.error).toHaveBeenCalledWith('telegram timeout', 5000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('webhook reports not-configured responses', async () => {
		const { useWebhookTest } = await import('./webhook/useWebhookTest.svelte');

		mockNotificationApi.testWebhook.mockResolvedValue({
			ok: false,
			configured: false
		});

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const webhook = useWebhookTest(() => '', { api: mockNotificationApi as never });

				void webhook.sendTest().then(() => {
					expect(mockNotifications.error).toHaveBeenCalledWith('webhook not configured', 5000);
					expect(webhook.lastResult).toEqual({ ok: false, configured: false });
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('webhook auto-swaps default payload for Discord URLs', async () => {
		const { useWebhookTest } = await import('./webhook/useWebhookTest.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let webhookUrl = $state('https://example.com/hook');
				const webhook = useWebhookTest(() => webhookUrl);

				expect(webhook.testText).toBe('{"event":"test","message":"Hello from ESP32"}');

				webhookUrl = 'https://discord.com/api/webhooks/123/abc';

				void flushPromises().then(() => {
					expect(webhook.testText).toBe('{"content":"Hello from ESP32"}');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('webhook preserves manual payload edits when URL changes', async () => {
		const { useWebhookTest } = await import('./webhook/useWebhookTest.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let webhookUrl = $state('https://example.com/hook');
				const webhook = useWebhookTest(() => webhookUrl);

				webhook.testText = '{"custom":true}';
				webhookUrl = 'https://discord.com/api/webhooks/123/abc';

				void flushPromises().then(() => {
					expect(webhook.testText).toBe('{"custom":true}');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('pushover trims text and surfaces API failures', async () => {
		const { usePushoverTest } = await import('./pushover/usePushoverTest.svelte');

		mockNotificationApi.testPushover.mockResolvedValue({
			ok: false,
			httpCode: 503
		});

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const pushover = usePushoverTest({ api: mockNotificationApi as never });
				pushover.testText = '  hello world  ';

				void pushover.sendTest().then(() => {
					expect(mockNotificationApi.testPushover).toHaveBeenCalledWith(
						'hello world',
						expect.any(AbortSignal)
					);
					expect(pushover.lastError).toBe('HTTP 503');
					expect(mockNotifications.error).toHaveBeenCalledWith('error: HTTP 503', 4000);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
