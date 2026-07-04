import { beforeEach, describe, expect, it, vi } from 'vitest';

type BleState = ReturnType<typeof import('./useBleDeviceSelect.svelte').useBleDeviceSelect>;

const { registerDestroy, runDestroy } = vi.hoisted(() => {
	let destroyCallbacks: Array<() => void> = [];

	return {
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
		onMount: (fn: () => void) => fn(),
		onDestroy: (fn: () => void) => registerDestroy(fn)
	};
});

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	alarms_select_ble_device: () => 'Select BLE device'
}));

vi.mock('$lib/utils', () => ({
	normalizeMac: (mac: string | null | undefined) => (mac ? mac.trim().toLowerCase() : '')
}));

describe('useBleDeviceSelect', () => {
	beforeEach(() => {
		runDestroy();
		vi.clearAllMocks();
	});

	it('starts BLE store and auto-selects the only saved sensor from websocket state', async () => {
		const { useBleDeviceSelect } = await import('./useBleDeviceSelect.svelte');
		const bleStore = {
			devices: {
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 50,
					batt: 90,
					rssi: -55,
					lastSeen: 1000,
					saved: true
				}
			},
			status: { enabled: true, running: true },
			settings: { sensors: [{ mac: 'aa:bb:cc:dd:ee:ff', alias: 'Desk sensor' }] },
			settingsLoading: false,
			settingsError: null,
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn()
		};

		let selectedMac = '';
		let bleState!: BleState;
		const cleanup = $effect.root(() => {
			bleState = useBleDeviceSelect(
				() => 'ble_temperature',
				() => selectedMac,
				(value) => {
					selectedMac = value;
				},
				{
					bleStore
				}
			);
		});

		await Promise.resolve();

		expect(bleStore.start).toHaveBeenCalledOnce();
		expect(selectedMac).toBe('aa:bb:cc:dd:ee:ff');
		expect(bleState.bleEnabled).toBe(true);
		expect(bleState.error).toBeNull();
		expect(bleState.options).toEqual([
			{ value: '', label: 'Select BLE device' },
			{ value: 'aa:bb:cc:dd:ee:ff', label: 'Desk sensor (21.5°C) ★' }
		]);

		runDestroy();
		expect(bleStore.stop).toHaveBeenCalledOnce();
		cleanup();
	});

	it('surfaces BLE store errors without REST fallback', async () => {
		const { useBleDeviceSelect } = await import('./useBleDeviceSelect.svelte');
		const bleStore = {
			devices: {},
			status: null,
			settings: null,
			settingsLoading: false,
			settingsError: 'BLE load failed',
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn()
		};

		let bleState!: BleState;
		const cleanup = $effect.root(() => {
			bleState = useBleDeviceSelect(
				() => 'ble_temperature',
				() => '',
				() => {},
				{
					bleStore
				}
			);
		});

		await Promise.resolve();

		expect(bleState.loading).toBe(false);
		expect(bleState.error).toBe('BLE load failed');
		expect(bleState.options).toEqual([{ value: '', label: 'Select BLE device' }]);

		runDestroy();
		cleanup();
	});

	it('keeps discovery-only devices out of alarm options even if they are present in the live store', async () => {
		const { useBleDeviceSelect } = await import('./useBleDeviceSelect.svelte');
		const bleStore = {
			devices: {
				'11:22:33:44:55:66': {
					mac: '11:22:33:44:55:66',
					temp: 19.2,
					humid: 41,
					batt: 80,
					rssi: -70,
					lastSeen: 900,
					saved: false
				},
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 50,
					batt: 90,
					rssi: -55,
					lastSeen: 1000,
					saved: true
				}
			},
			status: { enabled: true, running: true },
			settings: { sensors: [{ mac: 'aa:bb:cc:dd:ee:ff', alias: 'Desk sensor' }] },
			settingsLoading: false,
			settingsError: null,
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn()
		};

		let bleState!: BleState;
		const cleanup = $effect.root(() => {
			bleState = useBleDeviceSelect(
				() => 'ble_temperature',
				() => '',
				() => {},
				{
					bleStore
				}
			);
		});

		await Promise.resolve();

		expect(bleState.options).toEqual([
			{ value: '', label: 'Select BLE device' },
			{ value: 'aa:bb:cc:dd:ee:ff', label: 'Desk sensor (21.5°C) ★' }
		]);

		runDestroy();
		cleanup();
	});

	it('shows humidity in the device label when the alarm source is BLE humidity', async () => {
		const { useBleDeviceSelect } = await import('./useBleDeviceSelect.svelte');
		const bleStore = {
			devices: {
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 50,
					batt: 90,
					rssi: -55,
					lastSeen: 1000,
					saved: true
				}
			},
			status: { enabled: true, running: true },
			settings: { sensors: [{ mac: 'aa:bb:cc:dd:ee:ff', alias: 'Desk sensor' }] },
			settingsLoading: false,
			settingsError: null,
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn()
		};

		let bleState!: BleState;
		const cleanup = $effect.root(() => {
			bleState = useBleDeviceSelect(
				() => 'ble_humidity',
				() => '',
				() => {},
				{
					bleStore
				}
			);
		});

		await Promise.resolve();

		expect(bleState.options).toEqual([
			{ value: '', label: 'Select BLE device' },
			{ value: 'aa:bb:cc:dd:ee:ff', label: 'Desk sensor (50%) ★' }
		]);

		runDestroy();
		cleanup();
	});

	it('shows battery in the device label when the alarm source is BLE battery', async () => {
		const { useBleDeviceSelect } = await import('./useBleDeviceSelect.svelte');
		const bleStore = {
			devices: {
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 50,
					batt: 90,
					rssi: -55,
					lastSeen: 1000,
					saved: true
				}
			},
			status: { enabled: true, running: true },
			settings: { sensors: [{ mac: 'aa:bb:cc:dd:ee:ff', alias: 'Desk sensor' }] },
			settingsLoading: false,
			settingsError: null,
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn()
		};

		let bleState!: BleState;
		const cleanup = $effect.root(() => {
			bleState = useBleDeviceSelect(
				() => 'ble_battery',
				() => '',
				() => {},
				{
					bleStore
				}
			);
		});

		await Promise.resolve();

		expect(bleState.options).toEqual([
			{ value: '', label: 'Select BLE device' },
			{ value: 'aa:bb:cc:dd:ee:ff', label: 'Desk sensor (90%) ★' }
		]);

		runDestroy();
		cleanup();
	});

	it('shows RSSI in the device label when the alarm source is BLE RSSI', async () => {
		const { useBleDeviceSelect } = await import('./useBleDeviceSelect.svelte');
		const bleStore = {
			devices: {
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 50,
					batt: 90,
					rssi: -55,
					lastSeen: 1000,
					saved: true
				}
			},
			status: { enabled: true, running: true },
			settings: { sensors: [{ mac: 'aa:bb:cc:dd:ee:ff', alias: 'Desk sensor' }] },
			settingsLoading: false,
			settingsError: null,
			start: vi.fn(),
			stop: vi.fn(),
			refresh: vi.fn()
		};

		let bleState!: BleState;
		const cleanup = $effect.root(() => {
			bleState = useBleDeviceSelect(
				() => 'ble_rssi',
				() => '',
				() => {},
				{
					bleStore
				}
			);
		});

		await Promise.resolve();

		expect(bleState.options).toEqual([
			{ value: '', label: 'Select BLE device' },
			{ value: 'aa:bb:cc:dd:ee:ff', label: 'Desk sensor (-55 dBm) ★' }
		]);

		runDestroy();
		cleanup();
	});
});
