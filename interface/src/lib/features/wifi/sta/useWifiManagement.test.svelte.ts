import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useWifiManagement } from './useWifiManagement.svelte';

const {
	mockWifiApi,
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
		mockWifiApi: {
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

vi.mock('$lib/services/api/connectivity/WifiApiService', () => ({
	WifiApiService: class MockWifiApiService {
		constructor() {
			return mockWifiApi;
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
	wifi_apply_title: () => 'Apply WiFi Settings?',
	wifi_ap_mode_title: () => 'AP Mode Activation',
	wifi_off_mode_title: () => 'Turn WiFi Off?',
	wifi_sta_confirm_msg: () => 'Connect to saved networks',
	wifi_ap_confirm_msg: () => 'Switch to AP',
	wifi_off_confirm_msg: () => 'Switch WiFi off',
	wifi_apply_restart_btn: () => 'Apply & Restart',
	wifi_restart_ap_btn: () => 'Restart to AP',
	wifi_restart_off_btn: () => 'Restart with WiFi Off',
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
	wifi_sta_requires_network: () => 'Station mode requires at least one saved network.',
	toast_wifi_status_load_failed: () => 'Failed to load Wi-Fi status.',
	toast_wifi_settings_load_failed: () => 'Failed to load Wi-Fi settings.',
	toast_wifi_settings_update_failed: ({ error }: { error: string }) =>
		`Failed to update Wi-Fi settings: ${error}`,
	toast_wifi_settings_updated: () => 'Wi-Fi settings updated.',
	settings_save_error: () => 'Failed to save settings.',
	toast_request_timeout: () => 'Request timed out',
	settings_validation_error: () => 'Validation error',
	settings_saved_restarting: () => 'Saved, restarting',
	settings_saved: () => 'Saved',
	toast_message: ({ message }: { message: string }) => message
}));

describe('useWifiManagement', () => {
	beforeEach(() => {
		runDestroy();
		vi.clearAllMocks();
		vi.useRealTimers();
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
					connected: true,
					rssi: -42,
					reconnects: 1,
					ssid: 'Home',
					ip: '192.168.1.50',
					lastDisconnectReason: 0,
					healthy: true,
					mac: '00:11:22:33:44:55',
					bssid: '00:11:22:33:44:55',
					channel: 1,
					gateway: '192.168.1.1',
					subnet: '255.255.255.0',
					dns: '1.1.1.1'
				},
				runtime: {
					uptimeMs: 1234,
					uptimeHours: 0,
					loopCount: 1,
					slowLoops: 0
				}
			}
		} as never);
		mockWifiApi.getSettings.mockResolvedValue({
			hostname: 'node-a',
			mode: 'sta',
			wifi_networks: [{ ssid: 'Home', password: 'secret123', static_ip_config: false }]
		});
		mockWifiApi.saveSettings.mockImplementation(async (settings) => settings);
	});

	it('loads status from cached system_status data without forcing a duplicate first refresh', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement({
					bearerToken: 'token'
				});

				void wifiMgmt.loadInitialData().then(() => {
					expect(wifiMgmt.state.status.ssid).toBe('Home');
					expect(wifiMgmt.state.status.gateway_ip).toBe('192.168.1.1');
					expect(wifiMgmt.state.settings.hostname).toBe('node-a');
					expect(wifiMgmt.state.isSettingsDirty).toBe(false);
					expect(wifiMgmt.state.isSaveBlocked).toBe(false);
					expect(wifiMgmt.state.error).toBeNull();
					expect(mockSubscribeChannel).toHaveBeenCalledWith('system_status');
					expect(mockEnsureConnected).not.toHaveBeenCalled();
					expect(mockRequestSnapshot).not.toHaveBeenCalled();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('skips admin-only settings load for read-only users', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement(
					{
						bearerToken: 'token'
					},
					false
				);

				void wifiMgmt.loadInitialData().then(() => {
					expect(wifiMgmt.state.status.ssid).toBe('Home');
					expect(wifiMgmt.state.settingsLoading).toBe(false);
					expect(mockWifiApi.getSettings).not.toHaveBeenCalled();
					expect(wifiMgmt.settingsError).toBeNull();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('delegates save confirmation when Wi-Fi settings are valid', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement({
					bearerToken: 'token'
				});

				void wifiMgmt.loadInitialData().then(async () => {
					wifiMgmt.updateMode('ap');
					wifiMgmt.updateNetworks([]);

					wifiMgmt.saveSettings();

					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
					const [, options] = mockConfirmRestartAndSave.mock.calls[0];
					expect(options.title).toBe('AP Mode Activation');
					expect(options.message).toBe('Switch to AP');
					expect(options.confirmLabel).toBe('Restart to AP');

					const [onSave] = mockConfirmRestartAndSave.mock.calls[0];
					await onSave();

					expect(mockWifiApi.saveSettings).toHaveBeenCalledWith({
						hostname: 'node-a',
						mode: 'ap',
						wifi_networks: []
					});

					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('blocks save when hostname is empty', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement({
					bearerToken: 'token'
				});

				void wifiMgmt.loadInitialData().then(() => {
					wifiMgmt.updateHostname('');

					expect(wifiMgmt.state.isSettingsDirty).toBe(true);
					expect(wifiMgmt.state.isSaveBlocked).toBe(true);

					wifiMgmt.saveSettings();

					expect(mockConfirmRestartAndSave).not.toHaveBeenCalled();
					expect(mockWifiApi.saveSettings).not.toHaveBeenCalled();
					expect(mockNotifications.warning).toHaveBeenCalledWith('Validation error', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('blocks station mode when no saved networks remain', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement({
					bearerToken: 'token'
				});

				void wifiMgmt.loadInitialData().then(() => {
					wifiMgmt.updateNetworks([]);

					expect(wifiMgmt.state.isSettingsDirty).toBe(true);
					expect(wifiMgmt.state.isSaveBlocked).toBe(true);

					wifiMgmt.saveSettings();

					expect(mockConfirmRestartAndSave).not.toHaveBeenCalled();
					expect(mockWifiApi.saveSettings).not.toHaveBeenCalled();
					expect(mockNotifications.warning).toHaveBeenCalledWith('Validation error', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('blocks STA saves when a stored network becomes invalid', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement({
					bearerToken: 'token'
				});

				void wifiMgmt.loadInitialData().then(() => {
					wifiMgmt.updateNetworks([
						{
							ssid: 'Home',
							password: 'secret123',
							static_ip_config: true,
							local_ip: '192.168.1.10',
							subnet_mask: '255.255.255.0',
							gateway_ip: '192.168.1.1',
							dns_ip_1: '999.999.999.999'
						}
					]);

					wifiMgmt.saveSettings();

					expect(mockConfirmRestartAndSave).not.toHaveBeenCalled();
					expect(mockWifiApi.saveSettings).not.toHaveBeenCalled();
					expect(mockNotifications.warning).toHaveBeenCalledWith('Validation error', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('keeps status and settings errors separated when no system_status snapshot arrives', async () => {
		vi.useFakeTimers();
		mockGetSnapshot.mockReturnValue(null);

		let cleanup: (() => void) | undefined;
		const loaded = new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const wifiMgmt = useWifiManagement({
					bearerToken: 'token'
				});

				void wifiMgmt.loadInitialData().then(() => {
					expect(wifiMgmt.statusError).toBe('system status snapshot timed out');
					expect(wifiMgmt.settingsError).toBeNull();
					expect(wifiMgmt.state.error).toBe('system status snapshot timed out');
					expect(wifiMgmt.state.settings.hostname).toBe('node-a');
					expect(mockSubscribeChannel).toHaveBeenCalledWith('system_status');
					expect(mockEnsureConnected).not.toHaveBeenCalled();
					expect(mockRequestSnapshot).not.toHaveBeenCalled();
					resolve();
				});
			});
		});

		await vi.advanceTimersByTimeAsync(18000);
		await loaded;

		cleanup?.();
	});
});
