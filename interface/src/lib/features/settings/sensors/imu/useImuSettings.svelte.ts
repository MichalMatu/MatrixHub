import {
	DEFAULT_IMU_SETTINGS,
	ImuApiService,
	type ImuCalibrationResult,
	type ImuSettings,
	type ImuStatus
} from '$lib/services/api/integrations/ImuApiService';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import type { SettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { Logger } from '$lib/services/core/Logger';

type ImuErrors = Record<string, never>;

type ImuSettingsDeps = {
	api?: Pick<
		ImuApiService,
		| 'getSettings'
		| 'updateSettings'
		| 'getStatus'
		| 'calibrateOrientation'
		| 'resetOrientationBaseline'
	>;
	feedback?: SettingsFeedback<ImuSettings>;
	shouldLoad?: () => boolean;
};

function normalizeSettings(settings: Partial<ImuSettings> | null | undefined): ImuSettings {
	return {
		...DEFAULT_IMU_SETTINGS,
		...(settings ?? {}),
		orientation_baseline: {
			...DEFAULT_IMU_SETTINGS.orientation_baseline,
			...(settings?.orientation_baseline ?? {})
		}
	};
}

export function useImuSettings(deps: ImuSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const feedback = deps.feedback ?? createSettingsFeedback<ImuSettings>();
	let status = $state<ImuStatus | null>(null);
	let statusLoading = $state(false);
	let calibrating = $state(false);
	let resettingBaseline = $state(false);
	let calibrationResult = $state<ImuCalibrationResult | null>(null);
	let autoLoadArmed = true;
	let statusAbort: AbortController | null = null;

	function createApi() {
		return deps.api ?? apiClient!.createService(ImuApiService);
	}

	const settingsState = useSettings<ImuSettings, ImuErrors>(
		DEFAULT_IMU_SETTINGS,
		{},
		{
			load: async () => normalizeSettings(await createApi().getSettings()),
			save: async (settings) =>
				normalizeSettings(await createApi().updateSettings(normalizeSettings(settings))),
			feedback,
			onLoadSuccess: () => {
				void refreshStatus();
			},
			onSaveSuccess: () => {
				void refreshStatus();
			}
		}
	);

	async function refreshStatus() {
		statusAbort?.abort();
		statusAbort = new AbortController();
		statusLoading = true;
		try {
			status = await createApi().getStatus(statusAbort.signal);
		} catch (error) {
			if ((error as { name?: string })?.name !== 'AbortError') {
				Logger.warn('IMU status refresh failed', error);
			}
		} finally {
			statusLoading = false;
		}
	}

	async function calibrateOrientation() {
		if (calibrating) return;
		calibrating = true;
		try {
			calibrationResult = await createApi().calibrateOrientation();
			await settingsState.refreshSettings();
			await refreshStatus();
		} catch (error) {
			Logger.error('IMU orientation calibration failed', error);
			calibrationResult = {
				ok: false,
				status: 'request_failed',
				sample_count: 0,
				accel_magnitude_mean: 0,
				accel_magnitude_variance: 0,
				orientation_baseline: { x: 0, y: 0, z: 0 }
			};
		} finally {
			calibrating = false;
		}
	}

	async function resetOrientationBaseline() {
		if (resettingBaseline) return;
		resettingBaseline = true;
		try {
			await createApi().resetOrientationBaseline();
			calibrationResult = null;
			await settingsState.refreshSettings();
			await refreshStatus();
		} catch (error) {
			Logger.error('IMU orientation baseline reset failed', error);
		} finally {
			resettingBaseline = false;
		}
	}

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (shouldLoad === undefined) return;
		if (!shouldLoad) {
			autoLoadArmed = true;
			statusAbort?.abort();
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;
		void settingsState.loadSettings();
		void refreshStatus();
	});

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (!shouldLoad) return;
		const interval = window.setInterval(() => {
			void refreshStatus();
		}, 1500);
		return () => {
			window.clearInterval(interval);
			statusAbort?.abort();
		};
	});

	return {
		get settings() {
			return settingsState.settings;
		},
		get savedSettings() {
			return settingsState.savedSettings;
		},
		get loading() {
			return settingsState.loading;
		},
		get saving() {
			return settingsState.saving;
		},
		get hasChanges() {
			return settingsState.hasChanges;
		},
		get errorMessage() {
			return settingsState.errorMessage;
		},
		get status() {
			return status;
		},
		get statusLoading() {
			return statusLoading;
		},
		get calibrating() {
			return calibrating;
		},
		get resettingBaseline() {
			return resettingBaseline;
		},
		get calibrationResult() {
			return calibrationResult;
		},
		updateSetting: settingsState.updateSetting,
		saveSettings: settingsState.saveSettings,
		resetSettings: settingsState.resetSettings,
		refreshStatus,
		calibrateOrientation,
		resetOrientationBaseline
	};
}
