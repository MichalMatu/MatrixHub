/**
 * @file useWebhookSettings.svelte.ts
 * @brief Composable hook for Webhook settings management
 */

import {
	NotificationApiService,
	type NotificationSettings
} from '$lib/services/api/integrations/NotificationApiService';
import { isValidHttpUrl } from '$lib/utils';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

interface WebhookUIState {
	enabled: boolean;
	url: string;
}

interface WebhookErrors {
	url: boolean;
}

const DEFAULT_STATE: WebhookUIState = {
	enabled: false,
	url: ''
};

const DEFAULT_ERRORS: WebhookErrors = {
	url: false
};

type WebhookSettingsDeps = {
	api?: Pick<NotificationApiService, 'getSettings' | 'updateSettings'>;
	shouldLoad?: () => boolean;
};

export function useWebhookSettings(deps: WebhookSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();

	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}

	function createApi() {
		return deps.api ?? apiClient!.createService(NotificationApiService);
	}

	const feedback = createSettingsFeedback<WebhookUIState>();

	// 1. Define transformation and validation logic
	const hook = useSettings<WebhookUIState, WebhookErrors>(DEFAULT_STATE, DEFAULT_ERRORS, {
		// Load from API and map to UI state
		load: async () => {
			const data = await createApi().getSettings();
			return {
				enabled: data.webhook_enabled,
				url: data.webhook_url
			};
		},
		// Save UI state to API
		save: async (settings) => {
			const payload: Partial<NotificationSettings> = {
				webhook_enabled: settings.enabled,
				webhook_url: settings.url
			};
			const updated = await createApi().updateSettings(payload);
			return {
				enabled: updated.webhook_enabled,
				url: updated.webhook_url
			};
		},
		// Validation logic
		validate: (settings, errors) => {
			if (!settings.url) {
				errors.url = true;
				return true; // has error
			}
			if (!isValidHttpUrl(settings.url)) {
				errors.url = true;
				return true; // has error
			}
			return false;
		},
		shouldValidate: (settings) => settings.enabled,
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

	// 2. Computed properties specific to this module
	let canTest = $derived(
		hook.savedSettings?.enabled && hook.savedSettings.url && isValidHttpUrl(hook.savedSettings.url)
	);

	// 3. Return interface (spread hook result + specific additions)
	return {
		get settings() {
			return hook.settings;
		},
		get savedSettings() {
			return hook.savedSettings;
		},
		get errors() {
			return hook.errors;
		},
		get loading() {
			return hook.loading;
		},
		set loading(v: boolean) {
			hook.loading = v;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		loadSettings: hook.loadSettings,
		saveSettings: hook.saveSettings,
		updateSetting: (key: keyof WebhookUIState, value: WebhookUIState[keyof WebhookUIState]) => {
			if (key === 'url' && typeof value === 'string') {
				hook.updateSetting(key, filterAsciiPrintable(value));
				return;
			}
			hook.updateSetting(key, value);
		},
		resetSettings: hook.resetSettings,
		// Specifics
		get canTest() {
			return canTest;
		}
	};
}
