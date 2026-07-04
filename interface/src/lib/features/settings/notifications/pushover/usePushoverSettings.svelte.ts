/**
 * @file usePushoverSettings.svelte.ts
 * @brief Composable hook for Pushover settings
 */
import {
	NotificationApiService,
	type NotificationSettings
} from '$lib/services/api/integrations/NotificationApiService';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

interface PushoverState {
	pushover_enabled: boolean;
	pushover_user: string;
	pushover_token: string;
}

interface PushoverErrors {
	pushover_user: boolean;
	pushover_token: boolean;
}

const DEFAULT_STATE: PushoverState = {
	pushover_enabled: false,
	pushover_user: '',
	pushover_token: ''
};

const DEFAULT_ERRORS: PushoverErrors = {
	pushover_user: false,
	pushover_token: false
};

type PushoverSettingsDeps = {
	api?: Pick<NotificationApiService, 'getSettings' | 'updateSettings'>;
	shouldLoad?: () => boolean;
};

export function usePushoverSettings(deps: PushoverSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();

	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}

	function createApi() {
		return deps.api ?? apiClient!.createService(NotificationApiService);
	}

	const feedback = createSettingsFeedback<PushoverState>();

	const hook = useSettings<PushoverState, PushoverErrors>(DEFAULT_STATE, DEFAULT_ERRORS, {
		load: async () => {
			const data = await createApi().getSettings();
			return {
				pushover_enabled: data.pushover_enabled,
				pushover_user: data.pushover_user,
				pushover_token: data.pushover_token
			};
		},
		save: async (settings) => {
			const payload: Partial<NotificationSettings> = {
				pushover_enabled: settings.pushover_enabled,
				pushover_user: settings.pushover_user,
				pushover_token: settings.pushover_token,
				is_configured: true // Keep original logic
			};
			const updated = await createApi().updateSettings(payload);
			return {
				pushover_enabled: updated.pushover_enabled,
				pushover_user: updated.pushover_user,
				pushover_token: updated.pushover_token
			};
		},
		validate: (settings, errors) => {
			let hasError = false;
			if (!settings.pushover_user || settings.pushover_user.trim().length === 0) {
				errors.pushover_user = true;
				hasError = true;
			}
			if (!settings.pushover_token || settings.pushover_token.trim().length === 0) {
				errors.pushover_token = true;
				hasError = true;
			}
			return hasError;
		},
		shouldValidate: (settings) => settings.pushover_enabled,
		feedback
	});

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

	let canTest = $derived(
		hook.savedSettings?.pushover_enabled &&
			hook.savedSettings.pushover_user &&
			hook.savedSettings.pushover_token
	);

	return {
		get settings() {
			return hook.settings;
		},
		get errors() {
			return hook.errors;
		},
		get savedSettings() {
			return hook.savedSettings;
		},
		get canTest() {
			return canTest;
		},
		get loading() {
			return hook.loading;
		},
		set loading(value: boolean) {
			hook.loading = value;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		loadSettings: hook.loadSettings,
		saveSettings: hook.saveSettings,
		updateSetting: (key: keyof PushoverState, value: PushoverState[keyof PushoverState]) => {
			if ((key === 'pushover_user' || key === 'pushover_token') && typeof value === 'string') {
				hook.updateSetting(key, filterAsciiPrintable(value));
				return;
			}
			hook.updateSetting(key, value);
		},
		resetSettings: hook.resetSettings
	};
}
