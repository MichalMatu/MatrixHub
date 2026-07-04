import { untrack } from 'svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { BleApiService } from '$lib/services/api/connectivity/BleApiService';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { useBleSettings } from './useBleSettings.svelte';
import { useBleStatus } from './useBleStatus.svelte';

export function useBluetoothManagement() {
	const session = useSessionAccess();
	const { createService } = useApiClient();
	const api = createService(BleApiService);
	const bleStatus = useBleStatus();
	const bleSettings = useBleSettings(() => api);

	$effect(() => {
		if (!session.canRead) {
			return;
		}

		untrack(() => {
			bleStatus.start();
		});
		return () => {
			untrack(() => {
				bleStatus.stop();
			});
		};
	});

	async function saveSettings() {
		if (!bleSettings.hasChanges) return;

		try {
			await bleSettings.saveSettingsNow();
			// BLE uses a store-backed settings hook, so user-facing feedback lives
			// here in the management layer just like in other feature modules.
			notifications.success(m.settings_saved({ locale: i18n.languageTag }), 3000);
		} catch (nextError) {
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;

			const message = toUserRequestErrorMessage(nextError, {
				timeoutMessage: m.ble_error_settings_save_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.ble_error_settings_save_fallback({ locale: i18n.languageTag })
			});
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		}
	}

	function confirmSave() {
		void saveSettings();
	}

	return {
		get api() {
			return api;
		},
		get status() {
			return bleStatus.status;
		},
		get savedSettings() {
			return bleSettings.savedSettings;
		},
		get statusError() {
			return bleStatus.error;
		},
		get settingsError() {
			return bleSettings.error;
		},
		get saving() {
			return bleSettings.saving;
		},
		get hasChanges() {
			return bleSettings.hasChanges;
		},
		get isAdmin() {
			return bleSettings.isAdmin;
		},
		get canManage() {
			return session.canManage;
		},
		get isRunning() {
			return bleStatus.isRunning;
		},
		get isScannerActive() {
			return bleStatus.isScannerActive;
		},
		get localEnabled() {
			return bleSettings.localEnabled;
		},
		set localEnabled(value: boolean) {
			bleSettings.localEnabled = value;
		},
		confirmSave
	};
}
