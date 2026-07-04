import type { PowerConfig } from '$lib/types/system/power';

export const MS_PER_MIN = 60000;
export const INACTIVITY_MIN_MINUTES = 5;
export const INACTIVITY_MAX_MINUTES = 1440;
export const GRACE_MIN_MINUTES = 1;
export const GRACE_MAX_MINUTES = 10;
export const WAKE_INTERVAL_MIN_MINUTES = 1;
export const WAKE_INTERVAL_MAX_MINUTES = 1440;
export const TIMER_WAKE_AWAKE_MIN_MINUTES = 1;
export const TIMER_WAKE_AWAKE_MAX_MINUTES = 10;
export const BUTTON_WAKE_AWAKE_MIN_MINUTES = 1;
export const BUTTON_WAKE_AWAKE_MAX_MINUTES = 15;
export const TOUCH_GPIO_MIN = 1;
export const TOUCH_GPIO_MAX = 7;
export const TOUCH_THRESHOLD_MIN = 100;
export const TOUCH_THRESHOLD_MAX = 100000;

export type PowerSettingsDraft = PowerConfig;

export const POWER_CONFIG_KEYS: (keyof PowerSettingsDraft)[] = [
	'sleep_enabled',
	'inactivity_timeout_ms',
	'grace_after_boot_ms',
	'wake_timer_enabled',
	'wake_button_enabled',
	'wake_touch_enabled',
	'wake_interval_ms',
	'timer_wake_awake_ms',
	'button_wake_awake_ms',
	'wake_touch_gpio',
	'wake_touch_threshold'
];

export type PowerSettingsErrors = {
	inactivity_timeout_ms: boolean;
	grace_after_boot_ms: boolean;
	wake_interval_ms: boolean;
	timer_wake_awake_ms: boolean;
	button_wake_awake_ms: boolean;
	wake_touch_gpio: boolean;
	wake_touch_threshold: boolean;
	wake_sources: boolean;
};

export function toSettingsDraft(status: PowerConfig): PowerSettingsDraft {
	return {
		sleep_enabled: status.sleep_enabled,
		inactivity_timeout_ms: status.inactivity_timeout_ms,
		grace_after_boot_ms: status.grace_after_boot_ms,
		wake_timer_enabled: status.wake_timer_enabled,
		wake_button_enabled: status.wake_button_enabled,
		wake_touch_enabled: status.wake_touch_enabled,
		wake_interval_ms: status.wake_interval_ms,
		timer_wake_awake_ms: status.timer_wake_awake_ms,
		button_wake_awake_ms: status.button_wake_awake_ms,
		wake_touch_gpio: status.wake_touch_gpio,
		wake_touch_threshold: status.wake_touch_threshold
	};
}

export function createDefaultPowerSettingsErrors(): PowerSettingsErrors {
	return {
		inactivity_timeout_ms: false,
		grace_after_boot_ms: false,
		wake_interval_ms: false,
		timer_wake_awake_ms: false,
		button_wake_awake_ms: false,
		wake_touch_gpio: false,
		wake_touch_threshold: false,
		wake_sources: false
	};
}

export function hasEnabledWakeSource(
	settings: Pick<
		PowerSettingsDraft,
		'wake_timer_enabled' | 'wake_button_enabled' | 'wake_touch_enabled'
	>
): boolean {
	return settings.wake_timer_enabled || settings.wake_button_enabled || settings.wake_touch_enabled;
}

export function validatePowerSettings(
	settings: PowerSettingsDraft,
	errors: PowerSettingsErrors
): boolean {
	const inactivityMinutes = settings.inactivity_timeout_ms / MS_PER_MIN;
	const graceMinutes = settings.grace_after_boot_ms / MS_PER_MIN;
	const wakeIntervalMinutes = settings.wake_interval_ms / MS_PER_MIN;
	const timerAwakeMinutes = settings.timer_wake_awake_ms / MS_PER_MIN;
	const buttonAwakeMinutes = settings.button_wake_awake_ms / MS_PER_MIN;

	errors.inactivity_timeout_ms =
		!Number.isFinite(inactivityMinutes) ||
		inactivityMinutes < INACTIVITY_MIN_MINUTES ||
		inactivityMinutes > INACTIVITY_MAX_MINUTES;
	errors.grace_after_boot_ms =
		!Number.isFinite(graceMinutes) ||
		graceMinutes < GRACE_MIN_MINUTES ||
		graceMinutes > GRACE_MAX_MINUTES;
	errors.wake_interval_ms =
		!Number.isFinite(wakeIntervalMinutes) ||
		wakeIntervalMinutes < WAKE_INTERVAL_MIN_MINUTES ||
		wakeIntervalMinutes > WAKE_INTERVAL_MAX_MINUTES;
	errors.timer_wake_awake_ms =
		!Number.isFinite(timerAwakeMinutes) ||
		timerAwakeMinutes < TIMER_WAKE_AWAKE_MIN_MINUTES ||
		timerAwakeMinutes > TIMER_WAKE_AWAKE_MAX_MINUTES;
	errors.button_wake_awake_ms =
		!Number.isFinite(buttonAwakeMinutes) ||
		buttonAwakeMinutes < BUTTON_WAKE_AWAKE_MIN_MINUTES ||
		buttonAwakeMinutes > BUTTON_WAKE_AWAKE_MAX_MINUTES;
	errors.wake_touch_gpio =
		!Number.isInteger(settings.wake_touch_gpio) ||
		settings.wake_touch_gpio < TOUCH_GPIO_MIN ||
		settings.wake_touch_gpio > TOUCH_GPIO_MAX;
	errors.wake_touch_threshold =
		!Number.isInteger(settings.wake_touch_threshold) ||
		settings.wake_touch_threshold < TOUCH_THRESHOLD_MIN ||
		settings.wake_touch_threshold > TOUCH_THRESHOLD_MAX;
	errors.wake_sources = settings.sleep_enabled && !hasEnabledWakeSource(settings);

	return Object.values(errors).some(Boolean);
}

export function clampMinutes(value: number, min: number, max: number): number {
	if (!Number.isFinite(value)) return min;
	return Math.min(max, Math.max(min, Math.round(value)));
}

export function toMinutes(ms: number): number {
	return Math.max(0, Math.round(ms / MS_PER_MIN));
}

export function toMs(minutes: number): number {
	return Math.max(0, Math.round(minutes)) * MS_PER_MIN;
}
