import { NotificationApiService } from '$lib/services/api/integrations/NotificationApiService';
import { getRequestAbortKind } from '$lib/utils';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { getTestExecutionGate, mapTestExecutionError } from '../../testExecutionGate.svelte';

const TEST_TIMEOUT_MS = 35000;
const NOTIFICATION_DURATION_MS = 3000;
const ERROR_DURATION_MS = 5000;
const LONG_ERROR_DURATION_MS = 4000;

type TelegramTestResult = {
	ok: boolean;
	configured?: boolean;
	httpCode?: number;
	error?: string;
	tlsError?: string;
	response?: string;
};

type TelegramTestDeps = {
	api?: Pick<NotificationApiService, 'testTelegram'>;
};

export function useTelegramTest(deps: TelegramTestDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const testGate = getTestExecutionGate();
	let testText = $state<string>(m.telegram_test_default_message({ locale: i18n.languageTag }));
	let sending = $state(false);
	let lastResult = $state<TelegramTestResult | null>(null);
	let lastError = $state<string | null>(null);

	function createApi() {
		return deps.api ?? apiClient!.createService(NotificationApiService);
	}

	async function sendTest(): Promise<void> {
		if (sending || testGate.isBlocked) return;

		lastError = null;
		lastResult = null;

		const text = testText.trim();
		if (!text) {
			lastError = m.toast_telegram_test_message_required({ locale: i18n.languageTag });
			notifications.error(
				m.toast_message({ message: lastError }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
			return;
		}

		const finishGate = testGate.tryBeginRequest();
		if (!finishGate) {
			return;
		}

		sending = true;
		const controller = new AbortController();
		const timeoutId = window.setTimeout(() => controller.abort(), TEST_TIMEOUT_MS);

		try {
			const res = await createApi().testTelegram(text, controller.signal);
			lastResult = res;

			if (res.ok) {
				notifications.success(
					m.toast_telegram_test_sent({ locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
			} else if (res.configured === false) {
				notifications.error(
					m.toast_telegram_not_configured({ locale: i18n.languageTag }),
					ERROR_DURATION_MS
				);
			} else {
				const errorMessage =
					mapTestExecutionError(res.error) ??
					res.error ??
					m.toast_telegram_test_failed({ locale: i18n.languageTag });
				notifications.error(
					m.toast_message({ message: errorMessage }, { locale: i18n.languageTag }),
					LONG_ERROR_DURATION_MS
				);
			}
		} catch (e) {
			const msg = e instanceof Error ? e.message : String(e);
			const abortKind = getRequestAbortKind(e);
			if (abortKind) {
				lastError = m.toast_telegram_test_timeout({ locale: i18n.languageTag });
				notifications.error(
					m.toast_message({ message: lastError }, { locale: i18n.languageTag }),
					ERROR_DURATION_MS
				);
				return;
			}
			lastError = mapTestExecutionError(e) ?? msg;
			notifications.error(
				m.toast_message({ message: lastError }, { locale: i18n.languageTag }),
				LONG_ERROR_DURATION_MS
			);
		} finally {
			window.clearTimeout(timeoutId);
			sending = false;
			finishGate();
		}
	}

	return {
		get testText() {
			return testText;
		},
		set testText(value: string) {
			testText = value;
		},
		get sending() {
			return sending;
		},
		get blocked() {
			return sending || testGate.isBlocked;
		},
		get lastResult() {
			return lastResult;
		},
		get lastError() {
			return lastError;
		},
		sendTest
	};
}
