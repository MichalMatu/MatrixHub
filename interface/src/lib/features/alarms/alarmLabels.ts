import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { AlarmSource } from '$lib/types/domain/alarms';

export function getAlarmSourceLabel(source: AlarmSource): string {
	switch (source) {
		case 'co2':
			return m.source_co2({ locale: i18n.languageTag });
		case 'temperature':
			return m.source_temperature({ locale: i18n.languageTag });
		case 'humidity':
			return m.source_humidity({ locale: i18n.languageTag });
		case 'wifi_motion':
			return m.source_wifi_motion({ locale: i18n.languageTag });
		case 'wifi_csi_motion':
			return m.source_wifi_csi_motion({ locale: i18n.languageTag });
		case 'imu_tamper':
			return m.source_imu_tamper({ locale: i18n.languageTag });
		case 'gpio_digital':
			return m.source_gpio_digital({ locale: i18n.languageTag });
		case 'ble_temperature':
			return m.source_ble_temperature({ locale: i18n.languageTag });
		case 'ble_humidity':
			return m.source_ble_humidity({ locale: i18n.languageTag });
		case 'ble_battery':
			return m.source_ble_battery({ locale: i18n.languageTag });
		case 'ble_rssi':
			return m.source_ble_rssi({ locale: i18n.languageTag });
	}
}

export function getBooleanAlarmConditionLabel(source: AlarmSource): string {
	switch (source) {
		case 'wifi_csi_motion':
			return m.alarm_boolean_csi_motion_detected({ locale: i18n.languageTag });
		case 'imu_tamper':
			return m.alarm_boolean_imu_tamper_detected({ locale: i18n.languageTag });
		case 'gpio_digital':
			return m.alarm_boolean_gpio_true({ locale: i18n.languageTag });
		default:
			return getAlarmSourceLabel(source);
	}
}
