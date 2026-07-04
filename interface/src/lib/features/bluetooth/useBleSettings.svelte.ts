import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { Logger } from '$lib/services/core/Logger';
import type { BleApiService } from '$lib/services/api/connectivity/BleApiService';
import type { BleSettings } from '$lib/types/connectivity/ble';
import { bluetoothStore } from '$lib/stores/bluetooth.svelte';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

export function useBleSettings(getApi: () => BleApiService) {
	const session = useSessionAccess();
	let localEnabled = $state(false);
	let lastSyncedEnabled = $state<boolean | null>(null);

	let error = $state<string | null>(null);
	let saving = $state(false);

	async function fetchSettings() {
		bluetoothStore.refresh();
	}

	async function saveSettingsNow() {
		saving = true;
		try {
			const data = await getApi().saveSettings({
				enabled: localEnabled
			});
			bluetoothStore.setSettings(data);
			error = null;
			localEnabled = data.enabled;
			lastSyncedEnabled = data.enabled;
			bluetoothStore.refresh();
		} catch (nextError) {
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			Logger.error('Failed to save BLE settings:', nextError);
			const message = toUserRequestErrorMessage(nextError, {
				timeoutMessage: m.ble_error_settings_save_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.ble_error_settings_save_fallback({ locale: i18n.languageTag })
			});
			// Toast feedback is emitted by the Bluetooth management layer so the page
			// can keep one save-success / save-error path instead of mixing it into
			// this lower-level store-sync hook.
			error = message;
			throw nextError;
		} finally {
			saving = false;
		}
	}

	$effect(() => {
		const savedSettings = bluetoothStore.settings;
		if (!savedSettings) {
			return;
		}

		const shouldSyncLocalState = lastSyncedEnabled === null || localEnabled === lastSyncedEnabled;
		if (shouldSyncLocalState) {
			localEnabled = savedSettings.enabled;
		}
		lastSyncedEnabled = savedSettings.enabled;
		error = bluetoothStore.settingsError;
	});

	const hasChanges = $derived.by(() => {
		const savedSettings = bluetoothStore.settings;
		if (!savedSettings) return false;
		return localEnabled !== savedSettings.enabled;
	});

	const isAdmin = $derived(session.canManage);

	function resetSettings() {
		const savedSettings = bluetoothStore.settings;
		if (!savedSettings) return;
		localEnabled = savedSettings.enabled;
		lastSyncedEnabled = savedSettings.enabled;
		error = null;
	}

	return {
		get savedSettings() {
			return bluetoothStore.settings as BleSettings | null;
		},
		get localEnabled() {
			return localEnabled;
		},
		set localEnabled(value: boolean) {
			localEnabled = value;
		},
		get error() {
			return error ?? bluetoothStore.settingsError;
		},
		get saving() {
			return saving;
		},
		get hasChanges() {
			return hasChanges;
		},
		get isAdmin() {
			return isAdmin;
		},
		fetchSettings,
		saveSettingsNow,
		resetSettings
	};
}
