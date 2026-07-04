import type { MatrixApiService, MatrixSettings } from '$lib/services/api/core/MatrixApiService';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { DEFAULT_MATRIX_SETTINGS, type MatrixSettingsKey } from './matrixModel';

type MatrixErrors = Record<string, never>;

type MatrixSettingsDeps = {
	shouldLoad?: () => boolean;
	trackedKeys?: readonly MatrixSettingsKey[];
	saveKeys?: readonly MatrixSettingsKey[];
};

function pickMatrixSettings(
	settings: MatrixSettings,
	keys: readonly MatrixSettingsKey[]
): Partial<MatrixSettings> {
	return Object.fromEntries(
		keys.map((key) => [key, settings[key]] as const).filter(([, value]) => value !== undefined)
	) as Partial<MatrixSettings>;
}

function matrixSettingsEqualByKeys(
	keys: readonly MatrixSettingsKey[],
	current: MatrixSettings,
	saved: MatrixSettings
) {
	return (
		JSON.stringify(pickMatrixSettings(current, keys)) ===
		JSON.stringify(pickMatrixSettings(saved, keys))
	);
}

export function useMatrixSettings(
	apiFactory: () => MatrixApiService,
	deps: MatrixSettingsDeps = {}
) {
	const trackedKeys = deps.trackedKeys;
	const saveKeys = deps.saveKeys ?? trackedKeys;
	const feedback = createSettingsFeedback<MatrixSettings>({
		loadErrorFallback: () => m.matrix_err_load({ locale: i18n.languageTag }),
		saveErrorFallback: () => m.matrix_err_save({ locale: i18n.languageTag }),
		success: () => m.toast_settings_updated({ locale: i18n.languageTag })
	});

	const hook = useSettings<MatrixSettings, MatrixErrors>(
		DEFAULT_MATRIX_SETTINGS,
		{},
		{
			load: async () => {
				return await apiFactory().getSettings();
			},
			save: async (settings) => {
				return await apiFactory().updateSettings(
					saveKeys ? pickMatrixSettings(settings, saveKeys) : settings
				);
			},
			equals: trackedKeys
				? (current, saved) => matrixSettingsEqualByKeys(trackedKeys, current, saved)
				: undefined,
			feedback
		}
	);

	let autoLoadArmed = true;

	$effect(() => {
		if (deps.shouldLoad === undefined) return;
		if (!deps.shouldLoad()) {
			autoLoadArmed = true;
			hook.loading = false;
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;
		void hook.loadSettings();
	});

	return {
		get error() {
			return hook.errorMessage ?? undefined;
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
		get settings() {
			return hook.settings;
		},
		set settings(nextSettings: MatrixSettings) {
			hook.setSettings(nextSettings);
		},
		loadSettings: hook.loadSettings,
		saveSettingsNow: hook.saveSettingsNow,
		saveSettingsSilentlyNow: hook.saveSettingsSilentlyNow,
		saveSettings: hook.saveSettings,
		resetSettings: hook.resetSettings,
		updateSetting: hook.updateSetting
	};
}
