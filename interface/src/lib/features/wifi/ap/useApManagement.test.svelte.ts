import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useApManagement } from './useApManagement.svelte';

const {
	mockWifiApApi,
	mockConfirmRestartAndSave,
	mockNotifications,
	mockSubscribeChannel,
	mockUnsubscribeChannel,
	mockGetSnapshot,
	mockRequestSnapshot,
	mockEnsureConnected,
	mockEventSubscribe,
	registerDestroy,
	runDestroy
} = vi.hoisted(() => {
	let destroyCallbacks: Array<() => void> = [];

	return {
		mockWifiApApi: {
			getSettings: vi.fn(),
			saveSettings: vi.fn()
		},
		mockConfirmRestartAndSave: vi.fn(),
		mockNotifications: {
			success: vi.fn(),
			error: vi.fn(),
			warning: vi.fn()
		},
		mockSubscribeChannel: vi.fn(),
		mockUnsubscribeChannel: vi.fn(),
		mockGetSnapshot: vi.fn(),
		mockRequestSnapshot: vi.fn(),
		mockEnsureConnected: vi.fn(),
		mockEventSubscribe: vi.fn(() => vi.fn()),
		registerDestroy: (callback: () => void) => {
			destroyCallbacks.push(callback);
		},
		runDestroy: () => {
			for (const callback of destroyCallbacks) {
				callback();
			}
			destroyCallbacks = [];
		}
	};
});

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal<typeof import('svelte')>();
	return {
		...actual,
		onDestroy: (fn: () => void) => registerDestroy(fn)
	};
});

vi.mock('$lib/services/api/connectivity/WifiApApiService', () => ({
	WifiApApiService: class MockWifiApApiService {
		constructor() {
			return mockWifiApApi;
		}
	}
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: vi.fn()
	}
}));

vi.mock('$lib/utils/ui/restartConfirmation', () => ({
	confirmRestartAndSave: mockConfirmRestartAndSave
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: {
		subscribeChannel: mockSubscribeChannel,
		unsubscribeChannel: mockUnsubscribeChannel,
		getSnapshot: mockGetSnapshot,
		requestSnapshot: mockRequestSnapshot,
		ensureConnected: mockEnsureConnected,
		subscribeEvents: mockEventSubscribe
	},
	systemEvents: {
		subscribe: mockEventSubscribe
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
	wifi_password_error: () => 'WPA/WPA2 password must be 8-63 characters or empty',
	wifi_ssid_required: () => 'SSID is required',
	wifi_ssid_too_long: ({ max }: { max: number }) => `SSID too long (max ${max} bytes)`,
	wifi_hostname_error_min: ({ min }: { min: number }) =>
		`Hostname must be at least ${min} characters`,
	wifi_hostname_error_max: ({ max }: { max: number }) => `Hostname too long (max ${max})`,
	wifi_hostname_error_format: () => 'Hostname can only contain letters, numbers, and hyphens',
	wifi_validation_required: ({ field }: { field: string }) => `${field} is required`,
	wifi_validation_ipv4: ({ field }: { field: string }) => `${field} must be a valid IPv4 address`,
	wifi_field_local_ip: () => 'Local IP',
	wifi_field_subnet_mask: () => 'Subnet mask',
	wifi_field_gateway_ip: () => 'Gateway IP',
	wifi_field_dns_1: () => 'DNS 1',
	wifi_field_dns_2: () => 'DNS 2',
	wifi_networks_limit_error: ({ max }: { max: number }) => `Maximum ${max} networks allowed`,
	toast_ap_status_load_failed: () => 'Failed to load AP status.',
	toast_ap_settings_load_failed: () => 'Failed to load AP settings.',
	settings_save_error: () => 'Failed to save settings.',
	settings_validation_error: () => 'Validation error',
	settings_saved_restarting: () => 'Saved, restarting',
	ap_apply_title: () => 'Apply Access Point Settings?',
	ap_apply_msg: () => 'Restart to apply AP settings',
	wifi_apply_restart_btn: () => 'Apply & Restart',
	toast_request_timeout: () => 'Request timed out',
	toast_message: ({ message }: { message: string }) => message
}));

describe('useApManagement', () => {
	beforeEach(() => {
		runDestroy();
		vi.clearAllMocks();
		mockGetSnapshot.mockReturnValue({
			diagnostics: {
				healthy: true,
				heap: {
					free: 32000,
					min: 30000,
					largest: 24000,
					fragmentation: 5
				},
				wifi: {
					connected: false,
					reconnects: 1,
					lastDisconnectReason: 0,
					healthy: true
				},
				ap: {
					active: true,
					ip: '192.168.4.1',
					mac: '00:11:22:33:44:55',
					stationNum: 2
				},
				runtime: {
					uptimeMs: 1234,
					uptimeHours: 0,
					loopCount: 1,
					slowLoops: 0
				}
			}
		} as never);
		mockWifiApApi.getSettings.mockResolvedValue({
			ssid: 'ESP32-AP',
			password: 'secret123',
			channel: 6,
			ssid_hidden: false,
			max_clients: 4,
			local_ip: '192.168.4.1',
			gateway_ip: '192.168.4.1',
			subnet_mask: '255.255.255.0'
		});
		mockWifiApApi.saveSettings.mockImplementation(async (settings) => settings);
	});

	it('loads AP status from cached system_status data without forcing a duplicate first refresh', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const apMgmt = useApManagement({
					bearerToken: 'token'
				});

				void apMgmt.loadInitialData().then(() => {
					// 0 means "active" in the historical /rest/apStatus contract that
					// the AP card still consumes, so keep the websocket mapping aligned.
					expect(apMgmt.state.status.status).toBe(0);
					expect(apMgmt.state.status.ip_address).toBe('192.168.4.1');
					expect(apMgmt.state.status.mac_address).toBe('00:11:22:33:44:55');
					expect(apMgmt.state.settings?.ssid).toBe('ESP32-AP');
					expect(apMgmt.state.isSettingsDirty).toBe(false);
					expect(mockSubscribeChannel).toHaveBeenCalledWith('system_status');
					expect(mockEnsureConnected).not.toHaveBeenCalled();
					expect(mockRequestSnapshot).not.toHaveBeenCalled();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('skips admin-only AP settings load for read-only users', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const apMgmt = useApManagement(
					{
						bearerToken: 'token'
					},
					false
				);

				void apMgmt.loadInitialData().then(() => {
					expect(apMgmt.state.status.ip_address).toBe('192.168.4.1');
					expect(apMgmt.state.loading).toBe(false);
					expect(mockWifiApApi.getSettings).not.toHaveBeenCalled();
					expect(apMgmt.settingsError).toBeNull();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('delegates AP save flow to settings hook and restart confirmation', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const apMgmt = useApManagement({
					bearerToken: 'token'
				});

				void apMgmt.loadInitialData().then(async () => {
					if (!apMgmt.state.settings) throw new Error('settings not loaded');
					apMgmt.state.settings.ssid = 'ESP32-AP-2';

					apMgmt.saveSettings();

					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
					const [, options] = mockConfirmRestartAndSave.mock.calls[0];
					expect(options.title).toBe('Apply Access Point Settings?');
					expect(options.confirmLabel).toBe('Apply & Restart');

					const [onSave] = mockConfirmRestartAndSave.mock.calls[0];
					await onSave();

					expect(mockWifiApApi.saveSettings).toHaveBeenCalledWith(
						expect.objectContaining({ ssid: 'ESP32-AP-2' })
					);

					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('blocks AP saves when the settings payload is invalid', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const apMgmt = useApManagement({
					bearerToken: 'token'
				});

				void apMgmt.loadInitialData().then(() => {
					if (!apMgmt.state.settings) throw new Error('settings not loaded');
					apMgmt.state.settings.local_ip = '999.168.4.1';

					apMgmt.saveSettings();

					expect(mockConfirmRestartAndSave).not.toHaveBeenCalled();
					expect(mockWifiApApi.saveSettings).not.toHaveBeenCalled();
					expect(mockNotifications.warning).toHaveBeenCalledWith('Validation error', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
