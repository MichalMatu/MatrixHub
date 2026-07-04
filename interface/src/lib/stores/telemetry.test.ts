import { describe, it, expect, beforeEach } from 'vitest';
import { TelemetryStore } from './telemetry.svelte';
import type { Battery, RSSI } from '../types/domain/sensors';

describe('TelemetryStore', () => {
	let store: TelemetryStore;

	beforeEach(() => {
		store = new TelemetryStore();
	});

	it('should initialize with default values', () => {
		expect(store.rssi.rssi).toBe(0);
		expect(store.rssi.disconnected).toBe(true);
		expect(store.battery.soc).toBe(100);
	});

	it('should update RSSI when valid data is provided', () => {
		store.setRSSI({ rssi: -65, ssid: 'MyWiFi' } as RSSI);
		expect(store.rssi.rssi).toBe(-65);
		expect(store.rssi.ssid).toBe('MyWiFi');
		expect(store.rssi.disconnected).toBe(false);
	});

	it('should mark as disconnected when SSID is empty', () => {
		store.setRSSI({ rssi: -65, ssid: '' } as RSSI);
		expect(store.rssi.disconnected).toBe(true);
		expect(store.rssi.rssi).toBe(0);
	});

	it('should mark as disconnected when RSSI is 0', () => {
		store.setRSSI({ rssi: 0, ssid: 'MyWiFi' } as RSSI);
		expect(store.rssi.disconnected).toBe(true);
	});

	it('should handle string RSSI values', () => {
		store.setRSSI({ rssi: '-75', ssid: 'MyWiFi' } as unknown as RSSI);
		expect(store.rssi.rssi).toBe(-75);
		expect(store.rssi.disconnected).toBe(false);
	});

	it('should update battery state', () => {
		store.setBattery({ soc: 45, charging: true } as Battery);
		expect(store.battery.soc).toBe(45);
		expect(store.battery.charging).toBe(true);
	});
});
