import { describe, expect, it } from 'vitest';
import {
	createDefaultPowerSettingsErrors,
	validatePowerSettings,
	type PowerSettingsDraft
} from './powerConfigModel';

const baseSettings: PowerSettingsDraft = {
	sleep_enabled: true,
	inactivity_timeout_ms: 600000,
	grace_after_boot_ms: 120000,
	wake_timer_enabled: true,
	wake_button_enabled: false,
	wake_touch_enabled: false,
	wake_interval_ms: 300000,
	timer_wake_awake_ms: 60000,
	button_wake_awake_ms: 120000,
	wake_touch_gpio: 4,
	wake_touch_threshold: 2000
};

describe('powerConfigModel', () => {
	it('rejects auto-sleep when all wake sources are disabled', () => {
		const errors = createDefaultPowerSettingsErrors();
		const settings: PowerSettingsDraft = {
			...baseSettings,
			wake_timer_enabled: false,
			wake_button_enabled: false,
			wake_touch_enabled: false
		};

		expect(validatePowerSettings(settings, errors)).toBe(true);
		expect(errors.wake_sources).toBe(true);
	});

	it('allows all wake sources to be disabled while auto-sleep is off', () => {
		const errors = createDefaultPowerSettingsErrors();
		const settings: PowerSettingsDraft = {
			...baseSettings,
			sleep_enabled: false,
			wake_timer_enabled: false,
			wake_button_enabled: false,
			wake_touch_enabled: false
		};

		expect(validatePowerSettings(settings, errors)).toBe(false);
		expect(errors.wake_sources).toBe(false);
	});
});
