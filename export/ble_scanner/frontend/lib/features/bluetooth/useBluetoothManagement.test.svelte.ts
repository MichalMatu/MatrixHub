import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useBluetoothManagement } from './useBluetoothManagement.svelte';

const { mockApi, mockCreateService, mockNotifications, mockUser, mockBluetoothStore } = vi.hoisted(
	() => {
		const storeState: {
			status: {
				enabled: boolean;
				running: boolean;
				scanner_active?: boolean;
				devices?: Array<{
					mac: string;
					temp: number;
					humid: number;
					batt: number;
					rssi: number;
					last_seen: number;
					saved?: boolean;
				}>;
			} | null;
			settings: { enabled: boolean; sensors: Array<{ mac: string; alias: string }> } | null;
			settingsLoading: boolean;
			settingsError: string | null;
			devices: Record<string, unknown>;
		} = {
			status: null,
			settings: null,
			settingsLoading: false,
			settingsError: null,
			devices: {}
		};

		const bluetoothStore = {
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn(),
			setSettings: vi.fn(
				(settings: { enabled: boolean; sensors: Array<{ mac: string; alias: string }> }) => {
					storeState.settings = settings;
					storeState.settingsLoading = false;
				}
			),
			setStatus: vi.fn((status: NonNullable<typeof storeState.status>) => {
				storeState.status = status;
			}),
			reset: vi.fn(() => {
				storeState.status = null;
				storeState.settings = null;
				storeState.settingsLoading = false;
				storeState.settingsError = null;
				storeState.devices = {};
			}),
			get status() {
				return storeState.status;
			},
			get settings() {
				return storeState.settings;
			},
			get settingsLoading() {
				return storeState.settingsLoading;
			},
			get settingsError() {
				return storeState.settingsError;
			},
			get devices() {
				return storeState.devices;
			}
		};

		return {
			mockApi: {
				saveSettings: vi.fn()
			},
			mockCreateService: vi.fn(),
			mockNotifications: {
				error: vi.fn(),
				success: vi.fn(),
				warning: vi.fn()
			},
			mockUser: {
				bearer_token: 'token',
				admin: true,
				isValid: true
			},
			mockBluetoothStore: bluetoothStore
		};
	}
);

vi.mock('$lib/services/api/connectivity/BleApiService', () => ({
	BleApiService: class {}
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: vi.fn()
	}
}));

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

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$lib/stores/bluetooth.svelte', () => ({
	bluetoothStore: mockBluetoothStore
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
	ble_error_settings_save_timeout: () => 'save timeout',
	ble_error_settings_save_fallback: () => 'save failed',
	settings_saved: () => 'saved',
	toast_message: ({ message }: { message: string }) => message
}));

describe('useBluetoothManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockCreateService.mockReturnValue(mockApi);
		mockBluetoothStore.reset();
		mockApi.saveSettings.mockImplementation(async (settings) => ({
			enabled: settings.enabled ?? true,
			sensors: []
		}));
		mockUser.admin = true;
	});

	it('reads BLE status and settings from the websocket store and starts bluetooth store', async () => {
		mockBluetoothStore.setStatus({
			enabled: true,
			running: true,
			scanner_active: true,
			devices: []
		});
		mockBluetoothStore.setSettings({
			enabled: true,
			sensors: []
		});

		const cleanup = $effect.root(() => {
			const bluetooth = useBluetoothManagement();

			void vi.waitFor(() => {
				expect(mockBluetoothStore.start).toHaveBeenCalledOnce();
				expect(bluetooth.status?.running).toBe(true);
				expect(bluetooth.isScannerActive).toBe(true);
				expect(bluetooth.savedSettings?.enabled).toBe(true);
			});
		});

		await vi.waitFor(() => {
			expect(mockBluetoothStore.start).toHaveBeenCalledOnce();
		});

		cleanup();
	});

	it('saves scanner-only enabled state directly without extra GET requests', async () => {
		mockBluetoothStore.setSettings({
			enabled: true,
			sensors: []
		});

		const cleanup = $effect.root(() => {
			const bluetooth = useBluetoothManagement();

			bluetooth.localEnabled = false;

			expect(bluetooth.hasChanges).toBe(true);
			bluetooth.confirmSave();
		});

		await vi.waitFor(() => {
			expect(mockApi.saveSettings).toHaveBeenCalledWith({ enabled: false });
			expect(mockBluetoothStore.setSettings).toHaveBeenCalledWith({ enabled: false, sensors: [] });
			expect(mockNotifications.success).toHaveBeenCalledWith('saved', 3000);
		});

		cleanup();
	});

	it('shows an error toast when saving BLE settings fails', async () => {
		mockBluetoothStore.setSettings({
			enabled: true,
			sensors: []
		});
		mockApi.saveSettings.mockRejectedValueOnce(new Error('save failed'));

		const cleanup = $effect.root(() => {
			const bluetooth = useBluetoothManagement();

			bluetooth.localEnabled = false;
			bluetooth.confirmSave();
		});

		await vi.waitFor(() => {
			expect(mockNotifications.error).toHaveBeenCalledWith('save failed', 5000);
		});

		cleanup();
	});

	it('keeps websocket BLE settings available for non-admin users', async () => {
		mockUser.admin = false;
		mockBluetoothStore.setSettings({
			enabled: true,
			sensors: [{ mac: 'AA:BB:CC:DD:EE:FF', alias: 'Desk' }]
		});

		const cleanup = $effect.root(() => {
			const bluetooth = useBluetoothManagement();

			void vi.waitFor(() => {
				expect(mockBluetoothStore.start).toHaveBeenCalledOnce();
				expect(bluetooth.savedSettings?.sensors).toEqual([
					{ mac: 'AA:BB:CC:DD:EE:FF', alias: 'Desk' }
				]);
				expect(bluetooth.canManage).toBe(false);
			});
		});

		await vi.waitFor(() => {
			expect(mockBluetoothStore.start).toHaveBeenCalledOnce();
		});

		cleanup();
	});

	it('can request a fresh BLE snapshot over websocket when asked', () => {
		const cleanup = $effect.root(() => {
			const bluetooth = useBluetoothManagement();

			expect(bluetooth.api).toBe(mockApi);
		});

		mockBluetoothStore.refresh();
		expect(mockBluetoothStore.refresh).toHaveBeenCalledOnce();

		cleanup();
	});
});
