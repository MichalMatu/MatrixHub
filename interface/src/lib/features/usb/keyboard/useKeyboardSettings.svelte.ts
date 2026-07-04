import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type {
	KeyboardApiService,
	KeyboardConfig
} from '$lib/services/api/integrations/KeyboardApiService';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';

export function useKeyboardSettings(getApi: () => KeyboardApiService) {
	const session = useSessionAccess();
	let savedConfig = $state<KeyboardConfig | null>(null);
	let localEnabled = $state(false);
	let error = $state<string | null>(null);
	let saving = $state(false);

	function syncLocalState(config: KeyboardConfig) {
		localEnabled = config.enabled;
	}

	async function fetchSettings() {
		if (!session.canManage) return;

		try {
			const config = await getApi().getConfig();
			const shouldSyncLocalState = !savedConfig || !hasChanges;
			savedConfig = config;
			error = null;

			if (shouldSyncLocalState) {
				syncLocalState(config);
			}
		} catch (nextError) {
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			error = toUserRequestErrorMessage(nextError, {
				fallbackMessage: m.request_error_failed({ locale: i18n.languageTag })
			});
		}
	}

	async function saveSettingsNow() {
		saving = true;
		try {
			const config = await getApi().saveConfig({ enabled: localEnabled });
			savedConfig = config;
			error = null;
			syncLocalState(config);
		} catch (nextError) {
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			const message = toUserRequestErrorMessage(nextError, {
				fallbackMessage: m.request_error_failed({ locale: i18n.languageTag })
			});
			error = message;
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
			throw nextError;
		} finally {
			saving = false;
		}
	}

	function restoreSavedState() {
		if (!savedConfig) return;
		syncLocalState(savedConfig);
	}

	function confirmEnabledChange(nextEnabled: boolean) {
		if (!savedConfig) return;
		if (saving) return;

		localEnabled = nextEnabled;

		if (nextEnabled === savedConfig.enabled) return;

		// The keyboard page now saves directly from the header toggle.
		// If the restart modal is cancelled or the request fails, we must
		// snap the toggle back to the persisted value so the UI never
		// looks "dirty" without a Save button.
		confirmRestartAndSave(() => saveEnabledChange(nextEnabled), {
			message: m.restart_confirm_msg_generic({ locale: i18n.languageTag }),
			onCancel: restoreSavedState
		});
	}

	async function saveEnabledChange(nextEnabled: boolean) {
		localEnabled = nextEnabled;

		try {
			await saveSettingsNow();
		} catch (nextError) {
			restoreSavedState();
			throw nextError;
		}
	}

	const hasChanges = $derived.by(() => {
		if (!savedConfig) return false;
		return localEnabled !== savedConfig.enabled;
	});

	return {
		get savedConfig() {
			return savedConfig;
		},
		get localEnabled() {
			return localEnabled;
		},
		set localEnabled(value: boolean) {
			localEnabled = value;
		},
		get error() {
			return error;
		},
		get saving() {
			return saving;
		},
		get hasChanges() {
			return hasChanges;
		},
		fetchSettings,
		saveSettingsNow,
		confirmEnabledChange
	};
}
