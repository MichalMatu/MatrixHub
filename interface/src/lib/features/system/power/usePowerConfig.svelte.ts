import type { PowerApiService } from '$lib/services/api/core/PowerApiService';
import type { PowerConfig, PowerStatus } from '$lib/types/system/power';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import {
	POWER_CONFIG_KEYS,
	createDefaultPowerSettingsErrors,
	toSettingsDraft,
	validatePowerSettings,
	type PowerSettingsDraft,
	type PowerSettingsErrors
} from './powerConfigModel';

export function usePowerConfig(
	getApi: () => PowerApiService,
	getStatus: () => PowerStatus | null,
	applyConfig: (config: PowerConfig) => void
) {
	let errorMessage = $state<string | null>(null);
	let saving = $state(false);
	let savedSettings = $state<PowerSettingsDraft | null>(null);
	let settings = $state<PowerSettingsDraft>({
		sleep_enabled: false,
		inactivity_timeout_ms: 0,
		grace_after_boot_ms: 0,
		wake_timer_enabled: true,
		wake_button_enabled: true,
		wake_touch_enabled: false,
		wake_interval_ms: 0,
		timer_wake_awake_ms: 0,
		button_wake_awake_ms: 0,
		wake_touch_gpio: 4,
		wake_touch_threshold: 2000
	});
	let errors = $state<PowerSettingsErrors>(createDefaultPowerSettingsErrors());
	let initialized = $state(false);

	const hasChanges = $derived.by(() => {
		const saved = savedSettings;
		if (!saved) return false;
		return POWER_CONFIG_KEYS.some((key) => settings[key] !== saved[key]);
	});

	function clearErrors() {
		errors = createDefaultPowerSettingsErrors();
	}

	function syncFromStatus(status: PowerStatus | null) {
		if (!status) return;
		const nextSettings = toSettingsDraft(status);
		savedSettings = nextSettings;
		if (initialized && hasChanges) return;
		settings = nextSettings;
		clearErrors();
		errorMessage = null;
		initialized = true;
	}

	async function loadSettings() {
		syncFromStatus(getStatus());
	}

	function resetSettings() {
		if (!savedSettings) return;
		settings = savedSettings;
		clearErrors();
		errorMessage = null;
	}

	function toggleSleepEnabled() {
		settings.sleep_enabled = !settings.sleep_enabled;
		errors.wake_sources = false;
	}

	async function saveSettingsNow() {
		if (!savedSettings || !hasChanges) return false;

		clearErrors();
		if (validatePowerSettings(settings, errors)) {
			notifications.warning(m.settings_validation_error({ locale: i18n.languageTag }), 3000);
			return false;
		}

		saving = true;
		try {
			const nextConfig: PowerConfig = { ...settings };
			const savedConfig = await getApi().updateConfig(nextConfig);
			applyConfig(savedConfig);
			savedSettings = toSettingsDraft(savedConfig);
			settings = savedSettings;
			errorMessage = null;
			notifications.success(m.settings_saved({ locale: i18n.languageTag }), 3000);
			return true;
		} catch (nextError) {
			if (getRequestAbortKind(nextError) === 'abort') return false;
			const message = toUserRequestErrorMessage(nextError, {
				fallbackMessage: m.settings_save_error({ locale: i18n.languageTag })
			});
			errorMessage = message;
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 3000);
			return false;
		} finally {
			saving = false;
		}
	}

	return {
		get settings() {
			return settings;
		},
		get savedSettings() {
			return savedSettings;
		},
		get errors() {
			return errors;
		},
		get errorMessage() {
			return errorMessage;
		},
		get error() {
			return errorMessage;
		},
		get loading() {
			return !initialized && getStatus() === null;
		},
		get saving() {
			return saving;
		},
		get localSleepEnabled() {
			return settings.sleep_enabled;
		},
		set localSleepEnabled(value: boolean) {
			settings.sleep_enabled = value;
			errors.wake_sources = false;
		},
		get localInactivityTimeoutMs() {
			return settings.inactivity_timeout_ms;
		},
		set localInactivityTimeoutMs(value: number) {
			settings.inactivity_timeout_ms = value;
			errors.inactivity_timeout_ms = false;
		},
		get localGraceAfterBootMs() {
			return settings.grace_after_boot_ms;
		},
		set localGraceAfterBootMs(value: number) {
			settings.grace_after_boot_ms = value;
			errors.grace_after_boot_ms = false;
		},
		get localWakeTimerEnabled() {
			return settings.wake_timer_enabled;
		},
		set localWakeTimerEnabled(value: boolean) {
			settings.wake_timer_enabled = value;
			errors.wake_sources = false;
		},
		get localWakeButtonEnabled() {
			return settings.wake_button_enabled;
		},
		set localWakeButtonEnabled(value: boolean) {
			settings.wake_button_enabled = value;
			errors.wake_sources = false;
		},
		get localWakeTouchEnabled() {
			return settings.wake_touch_enabled;
		},
		set localWakeTouchEnabled(value: boolean) {
			settings.wake_touch_enabled = value;
			errors.wake_sources = false;
		},
		get localWakeIntervalMs() {
			return settings.wake_interval_ms;
		},
		set localWakeIntervalMs(value: number) {
			settings.wake_interval_ms = value;
			errors.wake_interval_ms = false;
		},
		get localTimerWakeAwakeMs() {
			return settings.timer_wake_awake_ms;
		},
		set localTimerWakeAwakeMs(value: number) {
			settings.timer_wake_awake_ms = value;
			errors.timer_wake_awake_ms = false;
		},
		get localButtonWakeAwakeMs() {
			return settings.button_wake_awake_ms;
		},
		set localButtonWakeAwakeMs(value: number) {
			settings.button_wake_awake_ms = value;
			errors.button_wake_awake_ms = false;
		},
		get localWakeTouchGpio() {
			return settings.wake_touch_gpio;
		},
		set localWakeTouchGpio(value: number) {
			settings.wake_touch_gpio = value;
			errors.wake_touch_gpio = false;
		},
		get localWakeTouchThreshold() {
			return settings.wake_touch_threshold;
		},
		set localWakeTouchThreshold(value: number) {
			settings.wake_touch_threshold = value;
			errors.wake_touch_threshold = false;
		},
		get hasChanges() {
			return hasChanges;
		},
		syncFromStatus,
		loadSettings,
		resetSettings,
		toggleSleepEnabled,
		saveSettingsNow,
		saveSettings: saveSettingsNow
	};
}
