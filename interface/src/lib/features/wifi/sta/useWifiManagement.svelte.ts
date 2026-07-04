import type {
	KnownNetworkItem,
	WifiMode,
	WifiSettings,
	WifiStatus
} from '$lib/types/connectivity/wifi';
import type { ApiClientOptions } from '$lib/utils';
import { useWifiStatus } from './useWifiStatus.svelte';
import { useWifiStaSettings } from './useWifiStaSettings.svelte';

export interface WifiState {
	status: WifiStatus;
	settings: WifiSettings;
	isSettingsDirty: boolean;
	isSaveBlocked: boolean;
	statusLoading: boolean;
	settingsLoading: boolean;
	error: string | null;
}

export function useWifiManagement(apiOptions: ApiClientOptions, canManage = true) {
	const wifiStatus = useWifiStatus();
	const wifiSettings = useWifiStaSettings(apiOptions);
	let statusLoading = $state(true);
	let settingsLoading = $state(true);

	let state = $state<WifiState>({
		get status() {
			return wifiStatus.status;
		},
		get settings() {
			return wifiSettings.settings;
		},
		get isSettingsDirty() {
			return wifiSettings.hasChanges;
		},
		get isSaveBlocked() {
			return wifiSettings.isSaveBlocked;
		},
		get statusLoading() {
			return statusLoading;
		},
		get settingsLoading() {
			return settingsLoading;
		},
		get error() {
			return wifiSettings.error ?? wifiStatus.error;
		}
	});

	function updateHostname(value: string) {
		wifiSettings.updateHostname(value);
	}

	function updateMode(value: WifiMode) {
		wifiSettings.updateMode(value);
	}

	function updateNetworks(networks: KnownNetworkItem[]) {
		wifiSettings.updateNetworks(networks);
	}

	async function loadInitialData() {
		statusLoading = true;
		settingsLoading = canManage;

		const statusPromise = wifiStatus.start().finally(() => {
			statusLoading = false;
		});

		if (!canManage) {
			await statusPromise;
			return;
		}

		const settingsPromise = wifiSettings.loadSettings().finally(() => {
			settingsLoading = false;
		});

		await Promise.allSettled([statusPromise, settingsPromise]);
	}

	return {
		get state() {
			return state;
		},
		get statusError() {
			return wifiStatus.error;
		},
		get settingsError() {
			return wifiSettings.error;
		},
		get statusLoading() {
			return statusLoading;
		},
		get settingsLoading() {
			return settingsLoading;
		},
		loadInitialData,
		fetchStatus: wifiStatus.refresh,
		saveSettings: wifiSettings.saveSettings,
		resetSettings: wifiSettings.resetSettings,
		updateHostname,
		updateMode,
		updateNetworks
	};
}
