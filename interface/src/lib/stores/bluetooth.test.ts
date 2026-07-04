import { describe, it, expect, vi, beforeEach } from 'vitest';
import { BluetoothStore } from './bluetooth.svelte';
import type { SystemEvent } from './system/types';

const { mockEvents } = vi.hoisted(() => {
	let _value: SystemEvent | null = null;
	const _subscribers = new Set<(v: SystemEvent | null) => void>();
	return {
		mockEvents: {
			subscribe: vi.fn((run: (v: SystemEvent | null) => void) => {
				run(_value);
				_subscribers.add(run);
				return () => _subscribers.delete(run);
			}),
			set: vi.fn((value: SystemEvent | null) => {
				_value = value;
				_subscribers.forEach((run) => run(value));
			})
		}
	};
});

const mockSystemStatus = {
	subscribeChannel: vi.fn(),
	unsubscribeChannel: vi.fn(),
	getSnapshot: vi.fn(() => null)
};

describe('BluetoothStore', () => {
	let store: BluetoothStore;

	beforeEach(() => {
		store = new BluetoothStore({
			statusStore: mockSystemStatus,
			eventBus: mockEvents
		});
		vi.clearAllMocks();
		mockEvents.set(null);
	});

	it('should initialize with empty devices', () => {
		expect(store.devices).toEqual({});
	});

	it('should activate channel only on first subscriber', () => {
		store.start();
		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('ble');
		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledTimes(1);

		store.start();
		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledTimes(1);
	});

	it('should deactivate channel only when last subscriber stops', () => {
		store.start();
		store.start();

		store.stop();
		expect(mockSystemStatus.unsubscribeChannel).not.toHaveBeenCalled();

		store.stop();
		expect(mockSystemStatus.unsubscribeChannel).toHaveBeenCalledWith('ble');
	});

	it('should update devices when receiving BLE events', () => {
		store.start();

		const mockDevice = {
			mac: 'AA:BB:CC:DD:EE:FF',
			temp: 25.5,
			humid: 40.2,
			batt: 90,
			rssi: -60,
			lastSeen: Date.now()
		};

		// Trigger event via mocked systemEvents
		mockEvents.set({
			type: 'ble',
			data: mockDevice
		});

		expect(store.devices['aa:bb:cc:dd:ee:ff']).toEqual({
			...mockDevice,
			mac: 'aa:bb:cc:dd:ee:ff'
		});
	});

	it('should merge data for existing devices', () => {
		const mac = '00:11:22:33:44:55';
		store.updateDevice({
			mac,
			temp: 20,
			humid: 50,
			batt: 100,
			rssi: -50,
			lastSeen: 1000,
			alias: 'Living Room'
		});

		store.updateDevice({
			mac,
			temp: 21,
			humid: 49,
			batt: 99,
			rssi: -55,
			lastSeen: 2000
		});

		expect(store.devices[mac].temp).toBe(21);
		expect(store.devices[mac].alias).toBe('Living Room'); // Preserved
	});

	it('should replace the root devices map on incremental updates', () => {
		const before = store.devices;

		store.updateDevice({
			mac: 'AA:BB:CC:DD:EE:FF',
			temp: 25.5,
			humid: 40.2,
			batt: 90,
			rssi: -60,
			lastSeen: 1234
		});

		expect(store.devices).not.toBe(before);
		expect(store.devices['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			temp: 25.5
		});
	});

	it('should reset devices', () => {
		// @ts-expect-error - testing partial data update
		store.updateDevice({ mac: '1', temp: 0 });
		store.reset();
		expect(store.devices).toEqual({});
	});

	it('should replace devices authoritatively from snapshot data', () => {
		store.updateDevice({
			mac: '00:11:22:33:44:55',
			temp: 20,
			humid: 40,
			batt: 100,
			rssi: -50,
			lastSeen: 1000
		});

		store.replaceDevices([
			{
				mac: 'aa:bb:cc:dd:ee:ff',
				temp: 21,
				humid: 41,
				batt: 99,
				rssi: -55,
				lastSeen: 2000
			}
		]);

		expect(store.devices['00:11:22:33:44:55']).toBeUndefined();
		expect(store.devices['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			temp: 21,
			lastSeen: 2000
		});
	});

	it('should overwrite stale saved flag from authoritative snapshot', () => {
		store.updateDevice({
			mac: 'AA:BB:CC:DD:EE:FF',
			temp: 20,
			humid: 40,
			batt: 100,
			rssi: -50,
			lastSeen: 1000,
			saved: true
		});

		store.replaceDevices([
			{
				mac: 'AA:BB:CC:DD:EE:FF',
				temp: 20.5,
				humid: 41,
				batt: 99,
				rssi: -48,
				lastSeen: 2000,
				saved: false
			}
		]);

		expect(store.devices['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			temp: 20.5,
			saved: false
		});
	});

	it('should keep both saved and discovered devices from a status snapshot', () => {
		store.replaceDevices([
			{
				mac: 'AA:BB:CC:DD:EE:FF',
				temp: 21.5,
				humid: 40,
				batt: 90,
				rssi: -50,
				lastSeen: 1000,
				saved: true
			},
			{
				mac: '11:22:33:44:55:66',
				temp: 18.2,
				humid: 47,
				batt: 65,
				rssi: -62,
				lastSeen: 1200,
				saved: false
			}
		]);

		expect(store.devices['aa:bb:cc:dd:ee:ff']?.saved).toBe(true);
		expect(store.devices['11:22:33:44:55:66']).toMatchObject({
			mac: '11:22:33:44:55:66',
			temp: 18.2,
			saved: false
		});
	});
});
