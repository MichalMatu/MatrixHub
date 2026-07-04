import type { ApSettings } from '$lib/types/connectivity/ap';
import { WifiApApiService } from '$lib/services/api/connectivity/WifiApApiService';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import type { ApiClientOptions } from '$lib/utils';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import {
	createWifiApFormErrors,
	validateWifiApSettings,
	type WifiApFormErrors
} from '$lib/features/wifi/wifiValidation';

const DEFAULT_SETTINGS: ApSettings = {
	ssid: '',
	password: '',
	channel: 1,
	ssid_hidden: false,
	max_clients: 4,
	local_ip: '',
	gateway_ip: '',
	subnet_mask: ''
};

function applyApValidation(settings: ApSettings, errors: WifiApFormErrors): boolean {
	const validation = validateWifiApSettings(settings);
	errors.ssid = validation.errors.ssid;
	errors.password = validation.errors.password;
	errors.channel = validation.errors.channel;
	errors.max_clients = validation.errors.max_clients;
	errors.local_ip = validation.errors.local_ip;
	errors.gateway_ip = validation.errors.gateway_ip;
	errors.subnet_mask = validation.errors.subnet_mask;
	return validation.valid;
}

export function useApSettings(apiOptions: ApiClientOptions) {
	const api = new WifiApApiService(apiOptions);
	const feedback = createSettingsFeedback<ApSettings>({
		loadErrorFallback: () => m.toast_ap_settings_load_failed({ locale: i18n.languageTag }),
		saveErrorFallback: () => m.settings_save_error({ locale: i18n.languageTag }),
		success: () => m.settings_saved_restarting({ locale: i18n.languageTag }),
		errorDurationMs: 5000
	});

	const hook = useSettings<ApSettings, WifiApFormErrors>(
		DEFAULT_SETTINGS,
		createWifiApFormErrors(),
		{
			load: () => api.getSettings(),
			save: (settings) => api.saveSettings(settings),
			validate: (settings, errors) => !applyApValidation(settings, errors),
			feedback
		}
	);

	function saveSettings(_settings?: ApSettings) {
		if (!hook.hasChanges) return;
		if (!applyApValidation(hook.settings, hook.errors)) {
			feedback.onValidationError?.();
			return;
		}

		confirmRestartAndSave(() => hook.saveSettingsNow(), {
			title: m.ap_apply_title({ locale: i18n.languageTag }),
			message: m.ap_apply_msg({ locale: i18n.languageTag }),
			confirmLabel: m.wifi_apply_restart_btn({ locale: i18n.languageTag }),
			useSleepInsteadOfRestart: false
		});
	}

	return {
		get settings() {
			return hook.settings;
		},
		get error() {
			return hook.errorMessage;
		},
		get loading() {
			return hook.loading;
		},
		set loading(value: boolean) {
			hook.loading = value;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		loadSettings: hook.loadSettings,
		refreshSettings: hook.refreshSettings,
		saveSettings,
		setSettings: hook.setSettings,
		resetSettings: hook.resetSettings
	};
}
