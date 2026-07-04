import type { Battery, RSSI } from '../types/domain/sensors';

export class TelemetryStore {
	rssi = $state({
		rssi: 0,
		ssid: '',
		disconnected: true
	});

	battery = $state({
		soc: 100,
		charging: false
	});

	setRSSI(data: RSSI) {
		const disconnected =
			(data.ssid ?? '') === '' || Number(data.rssi) === 0 || isNaN(Number(data.rssi));

		if (!disconnected) {
			this.rssi = { rssi: Number(data.rssi), ssid: data.ssid, disconnected: false };
		} else {
			this.rssi = { rssi: 0, ssid: data.ssid, disconnected: true };
		}
	}

	setBattery(data: Battery) {
		this.battery = { soc: data.soc, charging: data.charging };
	}
}

export const telemetry = new TelemetryStore();
