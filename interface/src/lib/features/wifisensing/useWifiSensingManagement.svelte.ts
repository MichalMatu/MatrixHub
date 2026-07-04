import { WifiSensingApiService } from '$lib/services/api/connectivity/WifiSensingApiService';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { useWifiSensingSettings } from './useWifiSensingSettings.svelte';
import { useWifiSensingStatus } from './useWifiSensingStatus.svelte';

export function useWifiSensingManagement() {
	const { createService } = useApiClient();
	const api = createService(WifiSensingApiService);
	const settings = useWifiSensingSettings(() => api);
	const status = useWifiSensingStatus(() => api, {
		isEnabled: () => settings.isEnabled,
		getThreshold: () => settings.appliedThreshold
	});

	$effect(() => {
		void settings.loadSettings();

		return () => {
			status.destroy();
		};
	});

	$effect(() => {
		if (settings.isEnabled) {
			status.start();
		} else {
			status.stop();
		}
	});

	async function saveSettings() {
		if (!settings.isAdmin || !settings.hasChanges) return;
		await settings.saveSettingsNow();
	}

	function resetSettings() {
		settings.resetSettings();
	}

	return {
		get sensingData() {
			return status.sensingData;
		},
		get savedSettings() {
			return settings.savedSettings;
		},
		get samples() {
			return status.samples;
		},
		get error() {
			return settings.error;
		},
		get saving() {
			return settings.saving;
		},
		get lastUpdate() {
			return status.lastUpdate;
		},
		get localEnabled() {
			return settings.localEnabled;
		},
		set localEnabled(value: boolean) {
			settings.localEnabled = value;
		},
		get localInterval() {
			return settings.localInterval;
		},
		set localInterval(value: number) {
			settings.localInterval = value;
		},
		get localThreshold() {
			return settings.localThreshold;
		},
		set localThreshold(value: number) {
			settings.localThreshold = value;
		},
		get appliedThreshold() {
			return settings.appliedThreshold;
		},
		get isAdmin() {
			return settings.isAdmin;
		},
		get isActive() {
			return status.isActive;
		},
		get motionDetected() {
			return status.motionDetected;
		},
		get isEnabled() {
			return settings.isEnabled;
		},
		get hasChanges() {
			return settings.hasChanges;
		},
		get hasUnsavedEnable() {
			return settings.hasUnsavedEnable;
		},
		saveSettings,
		resetSettings
	};
}
