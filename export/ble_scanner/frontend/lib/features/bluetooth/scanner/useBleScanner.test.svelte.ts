import { describe, expect, it } from 'vitest';
import type { BleSettings } from '$lib/types/connectivity/ble';
import { reconcileScannerData } from './useBleScanner.svelte';

describe('reconcileScannerData', () => {
	it('does not mark stale saved store entries as added when settings no longer contain the device', () => {
		const devices = {
			'aa:bb:cc:dd:ee:ff': {
				mac: 'aa:bb:cc:dd:ee:ff',
				temp: 21.5,
				humid: 50,
				batt: 80,
				rssi: -45,
				lastSeen: 1234,
				saved: true
			}
		};

		const settings: BleSettings = {
			enabled: true,
			sensors: []
		};

		const nextState = reconcileScannerData(devices, settings, true, {});

		expect(nextState.liveData).toEqual({});
		expect(nextState.scanResults['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			saved: false
		});
	});

	it('keeps discovered devices available to add in scan results', () => {
		const devices = {
			'11:22:33:44:55:66': {
				mac: '11:22:33:44:55:66',
				temp: 19.5,
				humid: 44,
				batt: 72,
				rssi: -58,
				lastSeen: 5678,
				saved: false
			}
		};

		const settings: BleSettings = {
			enabled: true,
			sensors: []
		};

		const nextState = reconcileScannerData(devices, settings, true, {});

		expect(nextState.liveData).toEqual({});
		expect(nextState.scanResults['11:22:33:44:55:66']).toMatchObject({
			mac: '11:22:33:44:55:66',
			temp: 19.5,
			saved: false
		});
	});

	it('keeps optimistic added state while settings refresh is still pending', () => {
		const devices = {
			'aa:bb:cc:dd:ee:ff': {
				mac: 'aa:bb:cc:dd:ee:ff',
				temp: 21.5,
				humid: 50,
				batt: 80,
				rssi: -45,
				lastSeen: 1234
			}
		};

		const settings: BleSettings = {
			enabled: true,
			sensors: []
		};

		const nextState = reconcileScannerData(devices, settings, true, {
			'aa:bb:cc:dd:ee:ff': {
				mac: 'aa:bb:cc:dd:ee:ff',
				temp: 21.5,
				humid: 50,
				batt: 80,
				rssi: -45,
				lastSeen: 1234,
				saved: true
			}
		});

		expect(nextState.scanResults['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			saved: true
		});
	});

	it('uses saved settings alias for whitelisted devices and keeps them in live data', () => {
		const devices = {
			'aa:bb:cc:dd:ee:ff': {
				mac: 'aa:bb:cc:dd:ee:ff',
				temp: 22.1,
				humid: 48,
				batt: 77,
				rssi: -40,
				lastSeen: 9999,
				alias: 'Raw alias'
			}
		};

		const settings: BleSettings = {
			enabled: true,
			sensors: [{ mac: 'AA:BB:CC:DD:EE:FF', alias: 'Kitchen' }]
		};

		const nextState = reconcileScannerData(devices, settings, true, {});

		expect(nextState.liveData['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			alias: 'Kitchen'
		});
		expect(nextState.scanResults['aa:bb:cc:dd:ee:ff']).toMatchObject({
			mac: 'aa:bb:cc:dd:ee:ff',
			alias: 'Kitchen',
			saved: true
		});
	});
});
