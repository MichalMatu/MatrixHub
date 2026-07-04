import { Logger } from '$lib/services/core/Logger';
import type { BleStatus } from '$lib/types/connectivity/ble';
import { bluetoothStore } from '$lib/stores/bluetooth.svelte';

export function useBleStatus() {
	function start() {
		bluetoothStore.start();
	}

	function stop() {
		bluetoothStore.stop();
	}

	function fetchStatus() {
		try {
			bluetoothStore.refresh();
		} catch (nextError) {
			Logger.error('Failed to refresh BLE status snapshot:', nextError);
		}
	}

	return {
		get status() {
			return bluetoothStore.status as BleStatus | null;
		},
		get error() {
			return null;
		},
		get isRunning() {
			return bluetoothStore.status?.running ?? false;
		},
		get isScannerActive() {
			return bluetoothStore.status?.scanner_active ?? false;
		},
		start,
		stop,
		fetchStatus
	};
}
