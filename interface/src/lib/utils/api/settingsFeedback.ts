import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { toUserRequestErrorMessage } from '$lib/utils';

const DEFAULT_NOTIFICATION_DURATION_MS = 3000;

export type SettingsFeedbackContext = 'load' | 'save';

export interface CreateSettingsFeedbackOptions<T> {
	loadErrorFallback?: () => string;
	saveErrorFallback?: () => string;
	success?: (settings: T) => string | false;
	validation?: () => string;
	loadErrorToast?: (message: string) => string;
	saveErrorToast?: (message: string) => string;
	errorDurationMs?: number;
	successDurationMs?: number;
	validationDurationMs?: number;
}

export interface SettingsFeedback<T> {
	resolveErrorMessage?: (error: unknown, context: SettingsFeedbackContext) => string;
	onError?: (params: { message: string; error: unknown; context: SettingsFeedbackContext }) => void;
	onValidationError?: () => void;
	onSaveSuccess?: (settings: T) => void;
}

export function createSettingsFeedback<T>(
	options: CreateSettingsFeedbackOptions<T> = {}
): SettingsFeedback<T> {
	function fallbackMessage(context: SettingsFeedbackContext): string {
		if (context === 'load') {
			return options.loadErrorFallback?.() ?? m.settings_load_error({ locale: i18n.languageTag });
		}

		return options.saveErrorFallback?.() ?? m.settings_save_error({ locale: i18n.languageTag });
	}

	return {
		resolveErrorMessage: (error, context) =>
			toUserRequestErrorMessage(error, {
				fallbackMessage: fallbackMessage(context)
			}),
		onError: ({ message, context }) => {
			const customToast =
				context === 'load' ? options.loadErrorToast?.(message) : options.saveErrorToast?.(message);
			notifications.error(
				customToast ?? m.toast_message({ message }, { locale: i18n.languageTag }),
				options.errorDurationMs ?? DEFAULT_NOTIFICATION_DURATION_MS
			);
		},
		onValidationError: () => {
			notifications.warning(
				options.validation?.() ?? m.settings_validation_error({ locale: i18n.languageTag }),
				options.validationDurationMs ?? DEFAULT_NOTIFICATION_DURATION_MS
			);
		},
		onSaveSuccess: (settings) => {
			const successMessage = options.success?.(settings);
			if (successMessage === false) return;
			notifications.success(
				successMessage ?? m.settings_saved({ locale: i18n.languageTag }),
				options.successDurationMs ?? DEFAULT_NOTIFICATION_DURATION_MS
			);
		}
	};
}
