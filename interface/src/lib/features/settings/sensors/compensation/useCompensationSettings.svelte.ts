/**
 * @file useCompensationSettings.svelte.ts
 * @brief Composable hook for SCD41 temperature compensation tuning
 */

import {
	CompensationApiService,
	type CompensationSettings
} from '$lib/services/api/integrations/CompensationApiService';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import type { SettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import {
	COMPENSATION_PRESETS,
	DEFAULT_COMPENSATION_SETTINGS,
	applyCompensationPreset,
	compensateHumidity,
	normalizeCompensationSettings,
	validateCompensationSettings,
	type CompensationPresetName
} from './compensationModel';

interface CompensationErrors {
	base_temp_offset: boolean;
	reference_cpu_temp: boolean;
	temp_offset_per_cpu_degree: boolean;
	min_temp_offset: boolean;
	max_temp_offset: boolean;
}

const DEFAULT_ERRORS: CompensationErrors = {
	base_temp_offset: false,
	reference_cpu_temp: false,
	temp_offset_per_cpu_degree: false,
	min_temp_offset: false,
	max_temp_offset: false
};

type CompensationSettingsDeps = {
	api?: Pick<CompensationApiService, 'getSettings' | 'updateSettings'>;
	feedback?: SettingsFeedback<CompensationSettings>;
	shouldLoad?: () => boolean;
};

export function useCompensationSettings(deps: CompensationSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const feedback = deps.feedback ?? createSettingsFeedback<CompensationSettings>();

	function createApi() {
		return deps.api ?? apiClient!.createService(CompensationApiService);
	}

	const hook = useSettings<CompensationSettings, CompensationErrors>(
		DEFAULT_COMPENSATION_SETTINGS,
		DEFAULT_ERRORS,
		{
			load: async () => {
				return normalizeCompensationSettings(await createApi().getSettings());
			},
			save: async (settings) => {
				return normalizeCompensationSettings(
					await createApi().updateSettings(normalizeCompensationSettings(settings))
				);
			},
			shouldValidate: (settings) => settings.enabled,
			validate: (settings, errors) => {
				const validation = validateCompensationSettings(settings);
				errors.base_temp_offset = validation.errors.base_temp_offset;
				errors.reference_cpu_temp = validation.errors.reference_cpu_temp;
				errors.temp_offset_per_cpu_degree = validation.errors.temp_offset_per_cpu_degree;
				errors.min_temp_offset = validation.errors.min_temp_offset;
				errors.max_temp_offset = validation.errors.max_temp_offset;
				return validation.hasError;
			},
			feedback
			// No restart config -> direct save
		}
	);

	let autoLoadArmed = true;

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (shouldLoad === undefined) return;
		if (!shouldLoad) {
			autoLoadArmed = true;
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;
		void hook.loadSettings();
	});

	let canPreview = $derived(Boolean(hook.savedSettings?.enabled));

	function applyPreset(name: CompensationPresetName) {
		hook.setSettings(applyCompensationPreset(hook.settings, name));
	}

	return {
		get settings() {
			return hook.settings;
		},
		get loading() {
			return hook.loading;
		},
		set loading(v: boolean) {
			hook.loading = v;
		},
		get savedSettings() {
			return hook.savedSettings;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		get canPreview() {
			return canPreview;
		},
		get errors() {
			return hook.errors;
		},
		get errorMessage() {
			return hook.errorMessage;
		},
		loadSettings: hook.loadSettings,
		saveSettings: hook.saveSettings,
		resetSettings: hook.resetSettings,
		updateSetting: hook.updateSetting,
		compensateHumidity,
		applyPreset,
		PRESETS: COMPENSATION_PRESETS
	};
}
