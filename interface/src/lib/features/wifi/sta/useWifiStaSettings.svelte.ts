import type { KnownNetworkItem, WifiMode, WifiSettings } from '$lib/types/connectivity/wifi';
import { WifiApiService } from '$lib/services/api/connectivity/WifiApiService';
import type { ApiClientOptions } from '$lib/utils';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import {
	createWifiStaSettingsErrors,
	WifiHostnameSchema,
	validateWifiStaSettings,
	type WifiStaSettingsErrors
} from '$lib/features/wifi/wifiValidation';

const DEFAULT_SETTINGS: WifiSettings = {
	hostname: '',
	mode: 'ap',
	wifi_networks: []
};

function applyStaValidation(settings: WifiSettings, errors: WifiStaSettingsErrors): boolean {
	const validation = validateWifiStaSettings(settings);
	errors.hostname = validation.errors.hostname;
	errors.wifi_networks = validation.errors.wifi_networks;
	return validation.valid;
}

function resolveRestartDialog(settings: WifiSettings) {
	if (settings.mode === 'sta') {
		return {
			title: m.wifi_apply_title(),
			message: m.wifi_sta_confirm_msg(),
			confirmLabel: m.wifi_apply_restart_btn()
		};
	}

	if (settings.mode === 'off') {
		return {
			title: m.wifi_off_mode_title(),
			message: m.wifi_off_confirm_msg(),
			confirmLabel: m.wifi_restart_off_btn()
		};
	}

	return {
		title: m.wifi_ap_mode_title(),
		message: m.wifi_ap_confirm_msg(),
		confirmLabel: m.wifi_restart_ap_btn()
	};
}

export function useWifiStaSettings(apiOptions: ApiClientOptions) {
	const api = new WifiApiService(apiOptions);
	const feedback = createSettingsFeedback<WifiSettings>({
		loadErrorFallback: () => m.toast_wifi_settings_load_failed({ locale: i18n.languageTag }),
		saveErrorFallback: () => m.settings_save_error({ locale: i18n.languageTag }),
		success: () => m.toast_wifi_settings_updated({ locale: i18n.languageTag }),
		saveErrorToast: (message) =>
			m.toast_wifi_settings_update_failed({ error: message }, { locale: i18n.languageTag }),
		errorDurationMs: 5000
	});

	const hook = useSettings<WifiSettings, WifiStaSettingsErrors>(
		DEFAULT_SETTINGS,
		createWifiStaSettingsErrors(),
		{
			load: () => api.getSettings(),
			save: (settings) => api.saveSettings(settings),
			validate: (settings, errors) => !applyStaValidation(settings, errors),
			feedback
		}
	);
	const isSaveBlocked = $derived.by(
		() =>
			!WifiHostnameSchema.safeParse(hook.settings.hostname).success ||
			(hook.settings.mode === 'sta' && hook.settings.wifi_networks.length === 0)
	);

	function saveSettings() {
		if (!hook.hasChanges) return;
		if (!applyStaValidation(hook.settings, hook.errors)) {
			feedback.onValidationError?.();
			return;
		}

		const dialog = resolveRestartDialog(hook.settings);
		confirmRestartAndSave(() => hook.saveSettingsNow(), {
			title: dialog.title,
			message: dialog.message,
			confirmLabel: dialog.confirmLabel,
			useSleepInsteadOfRestart: false
		});
	}

	function updateHostname(value: string) {
		hook.updateSetting('hostname', value);
	}

	function updateMode(value: WifiMode) {
		hook.updateSetting('mode', value);
	}

	function updateNetworks(networks: KnownNetworkItem[]) {
		hook.updateSetting('wifi_networks', networks);
	}

	return {
		get settings() {
			return hook.settings;
		},
		get errors() {
			return hook.errors;
		},
		get error() {
			return hook.errorMessage;
		},
		get loading() {
			return hook.loading;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		get isSaveBlocked() {
			return isSaveBlocked;
		},
		loadSettings: hook.loadSettings,
		refreshSettings: hook.refreshSettings,
		saveSettings,
		updateHostname,
		updateMode,
		updateNetworks,
		resetSettings: hook.resetSettings
	};
}
