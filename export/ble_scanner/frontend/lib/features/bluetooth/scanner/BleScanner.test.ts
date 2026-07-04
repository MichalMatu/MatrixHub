import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/svelte';
import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import BleScanner from './BleScanner.svelte';
import { bluetoothStore } from '$lib/stores/bluetooth.svelte';
import type { BleSensorConfig, BleSettings } from '$lib/types/connectivity/ble';

const { mockModals, mockNotifications } = vi.hoisted(() => ({
	mockModals: {
		open: vi.fn(),
		close: vi.fn()
	},
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn()
	}
}));

vi.mock('svelte-modals', () => ({
	modals: mockModals
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn((options: Record<string, unknown>) => mockModals.open({}, options)),
	prompt: vi.fn((options: Record<string, unknown>) => mockModals.open({}, options))
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn(() => 'request failed')
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', async (importOriginal) => {
	const actual = await importOriginal<typeof import('$lib/paraglide/messages.js')>();
	return {
		...actual,
		ble_my_devices: () => 'My devices',
		shelly_btn_add: () => 'Add',
		ble_no_paired: () => 'No paired devices',
		ble_scan_to_add: () => 'Scan to add',
		time_just_now: () => 'just now',
		time_ago_h: ({ h, m }: { h: number; m: number }) => `${h}h ${m}m ago`,
		time_ago_m: ({ m, s }: { m: number; s: number }) => `${m}m ${s}s ago`,
		time_ago_s: ({ s }: { s: number }) => `${s}s ago`,
		action_edit: () => 'Edit',
		action_delete_device: () => 'Delete device',
		menu_locked_admin: () => 'Administrator access required',
		tooltip_battery: () => 'Battery',
		tooltip_signal: () => 'Signal',
		status_waiting: () => 'Waiting',
		ble_scan_title: () => 'Scan devices',
		ble_scanning_msg: () => 'Scanning...',
		ble_scan_no_devices: () => 'No devices',
		dashboard_humid: () => 'Humidity',
		ble_scan_btn_add: () => 'Add',
		ble_scan_added: () => 'Added',
		action_close: () => 'Close',
		ble_scan_again: () => 'Scan again',
		ble_default_alias: () => 'Sensor',
		ble_msg_scanner_enabled: () => 'Scanner enabled',
		toast_ble_scan_start_timeout: () => 'scan timeout',
		toast_ble_scan_start_failed: () => 'scan failed',
		toast_message: ({ message }: { message: string }) => message,
		toast_ble_device_added: () => 'device added',
		ble_error_settings_save_fallback: () => 'save failed',
		ble_dialog_forget_title: () => 'Forget',
		ble_dialog_forget_msg: ({ mac }: { mac: string }) => `Forget ${mac}`,
		action_cancel: () => 'Cancel',
		action_delete: () => 'Delete',
		toast_ble_device_removed: () => 'device removed',
		ble_dialog_edit_alias_msg: ({ mac }: { mac: string }) => `Edit ${mac}`,
		toast_ble_device_updated: () => 'device updated'
	};
});

beforeAll(() => {
	Object.defineProperty(HTMLElement.prototype, 'animate', {
		configurable: true,
		writable: true,
		value: vi.fn(() => {
			const animation = {
				cancel: vi.fn(),
				finished: Promise.resolve(),
				onfinish: null as null | (() => void)
			};

			queueMicrotask(() => {
				animation.onfinish?.();
			});

			return animation;
		})
	});
});

function createSettings(sensors: BleSensorConfig[]): BleSettings {
	return {
		enabled: true,
		sensors
	};
}

function createDevice(
	overrides: Partial<(typeof bluetoothStore.devices)[string]> & { mac: string }
) {
	return {
		mac: overrides.mac,
		temp: overrides.temp ?? 21.5,
		humid: overrides.humid ?? 40,
		batt: overrides.batt ?? 90,
		rssi: overrides.rssi ?? -50,
		lastSeen: overrides.lastSeen ?? Date.now(),
		saved: overrides.saved,
		alias: overrides.alias
	};
}

describe('BleScanner integration', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		bluetoothStore.reset();
	});

	afterEach(() => {
		cleanup();
		bluetoothStore.reset();
	});

	it('shows Add instead of Added after removing a saved device and refreshing status', async () => {
		const mac = 'aa:bb:cc:dd:ee:ff';
		const initialSensor = { mac, alias: 'Kitchen' };

		const api = {
			startScan: vi.fn().mockResolvedValue(undefined),
			stopScan: vi.fn().mockResolvedValue(undefined),
			saveSettings: vi
				.fn()
				.mockImplementation(async ({ sensors }: { sensors: BleSensorConfig[] }) =>
					createSettings(sensors)
				)
		};

		bluetoothStore.setSettings(createSettings([initialSensor]));

		bluetoothStore.replaceDevices([
			createDevice({
				mac,
				temp: 21.5,
				humid: 40,
				batt: 90,
				rssi: -50,
				saved: true
			})
		]);

		render(BleScanner, {
			props: {
				api: api as never
			}
		});

		expect(screen.getByText('Kitchen')).toBeTruthy();

		await fireEvent.click(screen.getByRole('button', { name: 'Delete device' }));

		const deleteDialog = mockModals.open.mock.calls[0]?.[1] as { onConfirm?: () => Promise<void> };
		await deleteDialog.onConfirm?.();

		await waitFor(() => {
			expect(api.saveSettings).toHaveBeenCalledWith({ sensors: [] });
			expect(screen.queryByText('Kitchen')).toBeNull();
			expect(screen.getByText('No paired devices')).toBeTruthy();
		});

		bluetoothStore.replaceDevices([
			createDevice({
				mac,
				temp: 18.2,
				humid: 47,
				batt: 65,
				rssi: -62,
				saved: false
			})
		]);

		await fireEvent.click(screen.getByRole('button', { name: 'Scan to add' }));

		await waitFor(() => {
			expect(api.startScan).toHaveBeenCalledTimes(1);
			expect(screen.getByText('Scan devices')).toBeTruthy();
		});

		const resultRow = await waitFor(() => {
			const row = screen.getByText(mac).closest('li');
			expect(row).toBeTruthy();
			return row;
		});

		expect(within(resultRow!).getByRole('button', { name: 'Add' })).toBeTruthy();
		expect(within(resultRow!).queryByText('Added')).toBeNull();
	});

	it('shows discovered devices immediately after opening the scanner', async () => {
		const mac = '11:22:33:44:55:66';
		const api = {
			startScan: vi.fn().mockResolvedValue(undefined),
			stopScan: vi.fn().mockResolvedValue(undefined),
			saveSettings: vi.fn()
		};

		bluetoothStore.setSettings(createSettings([]));

		bluetoothStore.replaceDevices([
			createDevice({
				mac,
				temp: 19.4,
				humid: 46,
				batt: 72,
				rssi: -58,
				saved: false
			})
		]);

		render(BleScanner, {
			props: {
				api: api as never
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Scan to add' }));

		await waitFor(() => {
			expect(api.startScan).toHaveBeenCalledTimes(1);
			expect(screen.getByText('Scan devices')).toBeTruthy();
		});

		const resultRow = await waitFor(() => {
			const row = screen.getByText(mac).closest('li');
			expect(row).toBeTruthy();
			return row;
		});

		expect(within(resultRow!).getByRole('button', { name: 'Add' })).toBeTruthy();
		expect(within(resultRow!).queryByText('Added')).toBeNull();
		expect(within(resultRow!).getByText('19.4°C')).toBeTruthy();
	});

	it('shows placeholder time for saved devices when snapshot time is unavailable', async () => {
		const mac = 'aa:bb:cc:dd:ee:ff';
		const api = {
			startScan: vi.fn().mockResolvedValue(undefined),
			stopScan: vi.fn().mockResolvedValue(undefined),
			saveSettings: vi.fn()
		};

		bluetoothStore.setSettings(createSettings([{ mac, alias: 'Kitchen' }]));
		bluetoothStore.replaceDevices([
			createDevice({
				mac,
				temp: 21.5,
				humid: 40,
				batt: 90,
				rssi: -50,
				lastSeen: 0,
				saved: true
			})
		]);

		render(BleScanner, {
			props: {
				api: api as never
			}
		});

		await waitFor(() => {
			expect(screen.getByText('Kitchen')).toBeTruthy();
			expect(screen.getByText('--')).toBeTruthy();
		});
	});

	it('shows a read-only saved devices view without scan actions', async () => {
		const mac = '22:33:44:55:66:77';
		const api = {
			startScan: vi.fn().mockResolvedValue(undefined),
			stopScan: vi.fn().mockResolvedValue(undefined),
			saveSettings: vi.fn()
		};

		bluetoothStore.setSettings(createSettings([{ mac, alias: 'Office' }]));
		bluetoothStore.replaceDevices([
			createDevice({
				mac,
				temp: 20.2,
				humid: 43,
				batt: 81,
				rssi: -61,
				saved: true
			})
		]);

		render(BleScanner, {
			props: {
				api: api as never,
				canManage: false
			}
		});

		await waitFor(() => {
			expect(screen.getByText('Office')).toBeTruthy();
		});

		expect(screen.queryByRole('button', { name: 'Add' })).toBeNull();

		const editButton = screen.getByRole('button', { name: 'Edit' });
		const deleteButton = screen.getByRole('button', { name: 'Delete device' });

		expect((editButton as HTMLButtonElement).disabled).toBe(true);
		expect((deleteButton as HTMLButtonElement).disabled).toBe(true);
		expect(api.startScan).not.toHaveBeenCalled();
		expect(api.saveSettings).not.toHaveBeenCalled();
	});
});
