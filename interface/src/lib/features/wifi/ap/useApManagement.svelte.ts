import type { ApSettings, ApStatus } from '$lib/types/connectivity/ap';
import type { ApiClientOptions } from '$lib/utils';
import { useApStatus } from './useApStatus.svelte';
import { useApSettings } from './useApSettings.svelte';

export interface ApState {
	status: ApStatus;
	settings: ApSettings | null;
	isSettingsDirty: boolean;
	loading: boolean;
	error: string | null;
}

export function useApManagement(apiOptions: ApiClientOptions, canManage = true) {
	const apStatus = useApStatus();
	const apSettings = useApSettings(apiOptions);

	let state = $state<ApState>({
		get status() {
			return apStatus.status;
		},
		get settings() {
			return apSettings.settings;
		},
		get isSettingsDirty() {
			return apSettings.hasChanges;
		},
		get loading() {
			return apSettings.loading;
		},
		get error() {
			return apSettings.error ?? apStatus.error;
		}
	});

	async function loadInitialData() {
		if (!canManage) {
			apSettings.loading = false;
			await apStatus.start();
			return;
		}

		await Promise.allSettled([apStatus.start(), apSettings.loadSettings()]);
	}

	return {
		get state() {
			return state;
		},
		get statusError() {
			return apStatus.error;
		},
		get settingsError() {
			return apSettings.error;
		},
		loadInitialData,
		fetchStatus: apStatus.refresh,
		saveSettings: apSettings.saveSettings,
		resetSettings: apSettings.resetSettings
	};
}
