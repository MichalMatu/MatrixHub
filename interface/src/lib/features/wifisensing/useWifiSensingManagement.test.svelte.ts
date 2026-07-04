import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useWifiSensingManagement } from './useWifiSensingManagement.svelte';

const {
	mockApi,
	mockCreateService,
	mockUser,
	mockSubscribeChannel,
	mockUnsubscribeChannel,
	mockSystemEventsSubscribe,
	mockSystemEventsUnsubscribe,
	mockNotifications,
	setEventCallback
} = vi.hoisted(() => {
	let eventCallback: ((value: unknown) => void) | null = null;

	return {
		mockApi: {
			getSettings: vi.fn(),
			saveSettings: vi.fn(),
			getWifiStatus: vi.fn()
		},
		mockCreateService: vi.fn(),
		mockUser: {
			bearer_token: 'token',
			admin: true,
			isValid: true
		},
		mockSubscribeChannel: vi.fn(),
		mockUnsubscribeChannel: vi.fn(),
		mockSystemEventsUnsubscribe: vi.fn(),
		mockSystemEventsSubscribe: vi.fn((callback: (value: unknown) => void) => {
			eventCallback = callback;
			return mockSystemEventsUnsubscribe;
		}),
		mockNotifications: {
			success: vi.fn(),
			error: vi.fn()
		},
		setEventCallback: (value: unknown) => eventCallback?.(value)
	};
});

vi.mock('$app/state', () => ({
	page: {
		data: {
			features: {
				security: true
			}
		}
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

vi.mock('$lib/services/api/connectivity/WifiSensingApiService', () => ({
	WifiSensingApiService: class {}
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: vi.fn(),
		warn: vi.fn()
	}
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: {
		subscribeChannel: mockSubscribeChannel,
		unsubscribeChannel: mockUnsubscribeChannel,
		subscribeEvents: mockSystemEventsSubscribe
	},
	systemEvents: {
		subscribe: mockSystemEventsSubscribe
	}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown error';
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	settings_saved: () => 'saved',
	toast_message: ({ message }: { message: string }) => message,
	toast_wifisensing_settings_load_timeout: () => 'load timeout',
	toast_wifisensing_settings_load_failed: () => 'load failed',
	toast_wifisensing_settings_save_timeout: () => 'save timeout',
	toast_wifisensing_settings_save_failed: () => 'save failed'
}));

describe('useWifiSensingManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.useRealTimers();
		mockCreateService.mockReturnValue(mockApi);
		mockApi.getSettings.mockResolvedValue({
			enabled: true,
			sample_interval_ms: 750,
			variance_threshold: 6
		});
		mockApi.saveSettings.mockResolvedValue({
			enabled: false,
			sample_interval_ms: 750,
			variance_threshold: 6
		});
		mockApi.getWifiStatus.mockResolvedValue({
			status: 3,
			ssid: 'Office WiFi',
			channel: 11,
			rssi: -54
		});
	});

	it('loads settings even for non-admin users and starts sensing when enabled', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				mockUser.admin = false;
				const sensing = useWifiSensingManagement();

				void vi
					.waitFor(() => {
						expect(mockApi.getSettings).toHaveBeenCalledOnce();
						expect(sensing.savedSettings?.enabled).toBe(true);
						expect(sensing.isAdmin).toBe(false);
						expect(mockSubscribeChannel).toHaveBeenCalledWith('sensing');
					})
					.then(() => {
						resolve();
					});
			});
		});

		cleanup?.();
		expect(mockSystemEventsUnsubscribe).toHaveBeenCalledOnce();
	});

	it('ignores partial WiFi status when sensing context is offline', async () => {
		mockApi.getWifiStatus.mockResolvedValue({ status: 0 });
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				mockUser.admin = true;
				const sensing = useWifiSensingManagement();

				void vi
					.waitFor(() => {
						expect(sensing.savedSettings?.enabled).toBe(true);
					})
					.then(async () => {
						setEventCallback({
							type: 'sensing',
							data: { timestamp: 100, rssi: -60, variance: 1.25, motion: false }
						});

						await vi.waitFor(() => {
							expect(mockApi.getWifiStatus).toHaveBeenCalled();
							expect(sensing.sensingData?.connectedSSID).toBe('');
							expect(sensing.sensingData?.connectedChannel).toBe(0);
						});
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('updates live sensing stats from incoming events', async () => {
		vi.useFakeTimers();
		vi.setSystemTime(new Date('2026-03-18T10:00:00Z'));

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				mockUser.admin = true;
				const sensing = useWifiSensingManagement();

				void vi
					.waitFor(() => {
						expect(sensing.savedSettings?.enabled).toBe(true);
					})
					.then(async () => {
						setEventCallback({
							type: 'sensing',
							data: { timestamp: 100, rssi: -60, variance: 1.25, motion: false }
						});

						await vi.advanceTimersByTimeAsync(1000);

						setEventCallback({
							type: 'sensing',
							data: { timestamp: 200, rssi: -48, variance: 4.5, motion: true }
						});

						expect(sensing.samples).toHaveLength(2);
						expect(sensing.sensingData?.stats).toMatchObject({
							current: -48,
							min: -60,
							max: -48,
							avg: -54,
							variance: 4.5,
							sampleCount: 2,
							windowMs: 1000
						});
						expect(sensing.motionDetected).toBe(true);
						expect(sensing.sensingData?.connectedSSID).toBe('Office WiFi');
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('keeps applied threshold on saved settings until changes are persisted', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				mockUser.admin = true;
				const sensing = useWifiSensingManagement();

				void vi
					.waitFor(() => {
						expect(sensing.savedSettings?.variance_threshold).toBe(6);
					})
					.then(() => {
						sensing.localThreshold = 11;

						expect(sensing.hasChanges).toBe(true);
						expect(sensing.appliedThreshold).toBe(6);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('tracks toggle as unsaved change and persists it only on explicit save', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				mockUser.admin = true;
				const sensing = useWifiSensingManagement();

				void vi
					.waitFor(() => {
						expect(sensing.savedSettings?.enabled).toBe(true);
					})
					.then(async () => {
						sensing.localEnabled = false;

						expect(sensing.hasChanges).toBe(true);
						expect(mockApi.saveSettings).not.toHaveBeenCalled();

						await sensing.saveSettings();

						expect(mockApi.saveSettings).toHaveBeenCalledWith({
							enabled: false,
							sample_interval_ms: 750,
							variance_threshold: 6
						});
						expect(sensing.savedSettings?.enabled).toBe(false);
						expect(sensing.hasChanges).toBe(false);
						expect(mockUnsubscribeChannel).toHaveBeenCalledWith('sensing');

						setEventCallback({
							type: 'sensing',
							data: { timestamp: 999, rssi: -42, variance: 8.5, motion: true }
						});

						expect(sensing.samples).toHaveLength(0);
						expect(sensing.sensingData).toBeNull();
						resolve();
					});
			});
		});

		cleanup?.();
	});
});
