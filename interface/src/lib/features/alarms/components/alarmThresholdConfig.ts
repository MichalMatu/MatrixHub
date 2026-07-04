import type { AlarmSource } from '$lib/types/domain/alarms';

export interface ThresholdConfig {
	min: number;
	max: number;
	step: number;
	default: number;
}

/**
 * Get threshold configuration for a given alarm source
 */
export function getThresholdConfig(source: AlarmSource): ThresholdConfig {
	switch (source) {
		case 'co2':
			return { min: 400, max: 5000, step: 50, default: 1000 };
		case 'temperature':
			return { min: -20, max: 60, step: 1, default: 30 };
		case 'humidity':
			return { min: 0, max: 100, step: 1, default: 50 };
		case 'wifi_motion':
			return { min: 1, max: 20, step: 1, default: 10 };
		case 'wifi_csi_motion':
		case 'imu_tamper':
		case 'gpio_digital':
			return { min: 0, max: 1, step: 0.5, default: 0.5 };
		case 'ble_temperature':
			return { min: -20, max: 60, step: 1, default: 25 };
		case 'ble_humidity':
			return { min: 0, max: 100, step: 1, default: 50 };
		case 'ble_battery':
			return { min: 0, max: 100, step: 1, default: 20 };
		case 'ble_rssi':
			return { min: -120, max: -20, step: 1, default: -90 };
	}
}
