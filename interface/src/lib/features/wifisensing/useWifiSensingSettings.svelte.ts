import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import type { WifiSensingApiService } from '$lib/services/api/connectivity/WifiSensingApiService';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { WifiSensingSettings } from '$lib/types/connectivity/wifiSensing';
import { toUserRequestErrorMessage } from '$lib/utils';
import type { SettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';

const DEFAULT_SETTINGS: WifiSensingSettings = {
	enabled: false,
	sample_interval_ms: 250,
	variance_threshold: 4,
	csi_alarm: {
		enabled: false,
		bands: [],
		baseline_frames: 150,
		top_k: 8,
		enter_threshold: 6,
		clear_threshold: 3,
		hold_ms: 1200,
		clear_hold_ms: 2500,
		min_noise: 4,
		min_energy: 4,
		noisy_threshold: 80,
		auto_recalibration: true,
		sensitivity: 1
	}
};

type WifiSensingSettingsErrors = Record<string, never>;

export function useWifiSensingSettings(getApi: () => WifiSensingApiService) {
	const session = useSessionAccess();
	const feedback: SettingsFeedback<WifiSensingSettings> = {
		resolveErrorMessage: (error, context) =>
			toUserRequestErrorMessage(error, {
				timeoutMessage:
					context === 'load'
						? m.toast_wifisensing_settings_load_timeout({ locale: i18n.languageTag })
						: m.toast_wifisensing_settings_save_timeout({ locale: i18n.languageTag }),
				fallbackMessage:
					context === 'load'
						? m.toast_wifisensing_settings_load_failed({ locale: i18n.languageTag })
						: m.toast_wifisensing_settings_save_failed({ locale: i18n.languageTag })
			}),
		onError: ({ message, context }) => {
			if (context !== 'save') return;
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		},
		onSaveSuccess: () => {
			notifications.success(m.settings_saved({ locale: i18n.languageTag }), 3000);
		}
	};
	const settings = useSettings<WifiSensingSettings, WifiSensingSettingsErrors>(
		DEFAULT_SETTINGS,
		{},
		{
			load: () => getApi().getSettings(),
			save: (nextSettings) => getApi().saveSettings(nextSettings),
			feedback
		}
	);

	const isAdmin = $derived(session.canManage);
	const isEnabled = $derived(settings.savedSettings?.enabled ?? false);
	const hasUnsavedEnable = $derived(
		settings.settings.enabled && !(settings.savedSettings?.enabled ?? false)
	);
	const appliedThreshold = $derived(
		settings.savedSettings?.variance_threshold ?? settings.settings.variance_threshold
	);

	return {
		get settings() {
			return settings.settings;
		},
		get savedSettings() {
			return settings.savedSettings;
		},
		get errors() {
			return settings.errors;
		},
		get errorMessage() {
			return settings.errorMessage;
		},
		get error() {
			return settings.errorMessage;
		},
		get loading() {
			return settings.loading;
		},
		get saving() {
			return settings.saving;
		},
		get isAdmin() {
			return isAdmin;
		},
		get isEnabled() {
			return isEnabled;
		},
		get hasChanges() {
			return settings.hasChanges;
		},
		get hasUnsavedEnable() {
			return hasUnsavedEnable;
		},
		get appliedThreshold() {
			return appliedThreshold;
		},
		get localEnabled() {
			return settings.settings.enabled;
		},
		set localEnabled(value: boolean) {
			settings.updateSetting('enabled', value);
		},
		get localInterval() {
			return settings.settings.sample_interval_ms;
		},
		set localInterval(value: number) {
			settings.updateSetting('sample_interval_ms', value);
		},
		get localThreshold() {
			return settings.settings.variance_threshold;
		},
		set localThreshold(value: number) {
			settings.updateSetting('variance_threshold', value);
		},
		loadSettings: settings.loadSettings,
		saveSettingsNow: settings.saveSettingsNow,
		resetSettings: settings.resetSettings,
		refreshSettings: settings.refreshSettings,
		fetchSettings: settings.loadSettings,
		saveSettings: settings.saveSettings
	};
}
