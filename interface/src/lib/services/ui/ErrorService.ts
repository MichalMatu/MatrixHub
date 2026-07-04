import { notifications } from '$lib/components/toasts/notifications.svelte';
import { Logger } from '$lib/services/core/Logger';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

export class ErrorService {
	/**
	 * Handle an error by logging it and displaying a toast notification.
	 *
	 * @param error - The error object to handle (unknown type)
	 * @param contextMessage - Context for the error (e.g. "Failed to load settings")
	 * @param timeoutMs - Duration for the toast notification (default: 5000ms)
	 */
	static handle(error: unknown, contextMessage: string, timeoutMs: number = 5000): void {
		// Ignore abort errors (user cancelled request or page navigation)
		if (getRequestAbortKind(error) === 'abort') return;

		Logger.error(`${contextMessage}:`, error);

		const userMessage = toUserRequestErrorMessage(error, {
			fallbackMessage: contextMessage
		});

		notifications.error(
			m.toast_message({ message: userMessage }, { locale: i18n.languageTag }),
			timeoutMs
		);
	}

	/**
	 * Create a localized error handler function.
	 *
	 * @param contextMessage - The context message to use for errors
	 * @returns A function that accepts an error and handles it
	 */
	static createHandler(contextMessage: string) {
		return (error: unknown) => this.handle(error, contextMessage);
	}
}
