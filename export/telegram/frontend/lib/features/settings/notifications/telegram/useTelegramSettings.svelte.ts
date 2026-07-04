/**
 * @file useTelegramSettings.svelte.ts
 * @brief Composable hook for Telegram settings
 */
import {
	NotificationApiService,
	type NotificationSettings
} from '$lib/services/api/integrations/NotificationApiService';
import { isValidTelegramBotToken } from '$lib/utils';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

interface TelegramUIState {
	enabled: boolean;
	bot_token: string;
	chat_id: string;
	commands_enabled: boolean;
}

interface TelegramErrors {
	bot_token: boolean;
}

const DEFAULT_STATE: TelegramUIState = {
	enabled: false,
	bot_token: '',
	chat_id: '',
	commands_enabled: false
};

const DEFAULT_ERRORS: TelegramErrors = {
	bot_token: false
};

type TelegramSettingsDeps = {
	api?: Pick<NotificationApiService, 'getSettings' | 'updateSettings'>;
	shouldLoad?: () => boolean;
};

export function useTelegramSettings(deps: TelegramSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();

	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}

	function createApi() {
		return deps.api ?? apiClient!.createService(NotificationApiService);
	}

	const feedback = createSettingsFeedback<TelegramUIState>();

	const hook = useSettings<TelegramUIState, TelegramErrors>(DEFAULT_STATE, DEFAULT_ERRORS, {
		load: async () => {
			const settings = await createApi().getSettings();
			return {
				enabled: settings.telegram_enabled,
				bot_token: settings.bot_token,
				chat_id: settings.chat_id,
				commands_enabled: settings.commands_enabled
			};
		},
		save: async (settings) => {
			const payload: Partial<NotificationSettings> = {
				telegram_enabled: settings.enabled,
				bot_token: settings.bot_token,
				chat_id: settings.chat_id,
				commands_enabled: settings.commands_enabled
			};
			const updated = await createApi().updateSettings(payload);
			return {
				enabled: updated.telegram_enabled,
				bot_token: updated.bot_token,
				chat_id: updated.chat_id,
				commands_enabled: updated.commands_enabled
			};
		},
		validate: (settings, errors) => {
			if (!isValidTelegramBotToken(settings.bot_token)) {
				errors.bot_token = true;
				return true;
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

	let canTest = $derived(
		hook.savedSettings?.enabled &&
			isValidTelegramBotToken(hook.savedSettings.bot_token) &&
			hook.savedSettings.chat_id
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
		refreshSettings: hook.refreshSettings,
		saveSettings: hook.saveSettings,
		updateSetting: (key: keyof TelegramUIState, value: TelegramUIState[keyof TelegramUIState]) => {
			if ((key === 'bot_token' || key === 'chat_id') && typeof value === 'string') {
				hook.updateSetting(key, filterAsciiPrintable(value));
				return;
			}
			hook.updateSetting(key, value);
		},
		resetSettings: hook.resetSettings
	};
}
