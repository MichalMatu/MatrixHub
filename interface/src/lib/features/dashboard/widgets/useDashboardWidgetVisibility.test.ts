import { describe, expect, it } from 'vitest';
import { resolveDashboardWidgetVisibility } from './useDashboardWidgetVisibility.svelte';

describe('resolveDashboardWidgetVisibility', () => {
	it('hides all widgets before the system status summary is resolved', () => {
		expect(resolveDashboardWidgetVisibility(false, null)).toEqual({
			showBle: false,
			showShelly: false,
			showAlarms: false,
			showWifiSensing: false
		});
	});

	it('keeps backward compatibility when older firmware does not send dashboard_widgets', () => {
		expect(resolveDashboardWidgetVisibility(true, null)).toEqual({
			showBle: true,
			showShelly: true,
			showAlarms: true,
			showWifiSensing: true
		});
	});

	it('shows only widgets that are configured or enabled', () => {
		expect(
			resolveDashboardWidgetVisibility(true, {
				ble: { enabled: false, sensor_count: 2 },
				shelly: { device_count: 0 },
				alarms: { rule_count: 1 },
				wifi_sensing: { enabled: false }
			})
		).toEqual({
			showBle: true,
			showShelly: false,
			showAlarms: true,
			showWifiSensing: false
		});
	});

	it('shows BLE when scanner is enabled even before sensors are configured', () => {
		expect(
			resolveDashboardWidgetVisibility(true, {
				ble: { enabled: true, sensor_count: 0 },
				shelly: { device_count: 0 },
				alarms: { rule_count: 0 },
				wifi_sensing: { enabled: true }
			})
		).toEqual({
			showBle: true,
			showShelly: false,
			showAlarms: false,
			showWifiSensing: true
		});
	});
});
