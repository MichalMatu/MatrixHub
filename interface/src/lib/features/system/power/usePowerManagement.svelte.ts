/**
 * Power management state and API logic (Svelte 5 runes)
 */

import { PowerApiService } from '$lib/services/api/core/PowerApiService';
import { usePowerStatus } from './usePowerStatus.svelte';
import { usePowerConfig } from './usePowerConfig.svelte';
import { untrack } from 'svelte';

export function usePowerManagement(getApi: () => PowerApiService) {
	const powerStatus = usePowerStatus(getApi);
	const powerConfig = usePowerConfig(getApi, () => powerStatus.status, powerStatus.applyConfig);

	$effect(() => {
		const status = powerStatus.status;
		untrack(() => powerConfig.syncFromStatus(status));
	});

	async function restart() {
		await getApi().restart();
	}

	async function factoryReset() {
		await getApi().factoryReset();
	}

	async function requestSleep() {
		await getApi().requestSleep();
	}

	async function requestHygieneSleep() {
		await getApi().requestHygieneSleep();
	}

	return {
		get settings() {
			return powerConfig.settings;
		},
		get savedSettings() {
			return powerConfig.savedSettings;
		},
		get errors() {
			return powerConfig.errors;
		},
		get status() {
			return powerStatus.status;
		},
		get error() {
			return powerConfig.error ?? powerStatus.error;
		},
		get statusError() {
			return powerStatus.error;
		},
		get configError() {
			return powerConfig.error;
		},
		get loading() {
			return powerStatus.loading;
		},
		get saving() {
			return powerConfig.saving;
		},
		get localSleepEnabled() {
			return powerConfig.localSleepEnabled;
		},
		set localSleepEnabled(value: boolean) {
			powerConfig.localSleepEnabled = value;
		},
		get localInactivityTimeoutMs() {
			return powerConfig.localInactivityTimeoutMs;
		},
		set localInactivityTimeoutMs(value: number) {
			powerConfig.localInactivityTimeoutMs = value;
		},
		get localGraceAfterBootMs() {
			return powerConfig.localGraceAfterBootMs;
		},
		set localGraceAfterBootMs(value: number) {
			powerConfig.localGraceAfterBootMs = value;
		},
		get localWakeTimerEnabled() {
			return powerConfig.localWakeTimerEnabled;
		},
		set localWakeTimerEnabled(value: boolean) {
			powerConfig.localWakeTimerEnabled = value;
		},
		get localWakeButtonEnabled() {
			return powerConfig.localWakeButtonEnabled;
		},
		set localWakeButtonEnabled(value: boolean) {
			powerConfig.localWakeButtonEnabled = value;
		},
		get localWakeTouchEnabled() {
			return powerConfig.localWakeTouchEnabled;
		},
		set localWakeTouchEnabled(value: boolean) {
			powerConfig.localWakeTouchEnabled = value;
		},
		get localWakeIntervalMs() {
			return powerConfig.localWakeIntervalMs;
		},
		set localWakeIntervalMs(value: number) {
			powerConfig.localWakeIntervalMs = value;
		},
		get localTimerWakeAwakeMs() {
			return powerConfig.localTimerWakeAwakeMs;
		},
		set localTimerWakeAwakeMs(value: number) {
			powerConfig.localTimerWakeAwakeMs = value;
		},
		get localButtonWakeAwakeMs() {
			return powerConfig.localButtonWakeAwakeMs;
		},
		set localButtonWakeAwakeMs(value: number) {
			powerConfig.localButtonWakeAwakeMs = value;
		},
		get localWakeTouchGpio() {
			return powerConfig.localWakeTouchGpio;
		},
		set localWakeTouchGpio(value: number) {
			powerConfig.localWakeTouchGpio = value;
		},
		get localWakeTouchThreshold() {
			return powerConfig.localWakeTouchThreshold;
		},
		set localWakeTouchThreshold(value: number) {
			powerConfig.localWakeTouchThreshold = value;
		},
		get hasChanges() {
			return powerConfig.hasChanges;
		},
		loadSettings: powerConfig.loadSettings,
		resetSettings: powerConfig.resetSettings,
		fetchStatus: powerStatus.fetchStatus,
		toggleSleepEnabled: powerConfig.toggleSleepEnabled,
		saveSettingsNow: powerConfig.saveSettingsNow,
		saveSettings: powerConfig.saveSettings,
		restart,
		factoryReset,
		requestSleep,
		requestHygieneSleep
	};
}
