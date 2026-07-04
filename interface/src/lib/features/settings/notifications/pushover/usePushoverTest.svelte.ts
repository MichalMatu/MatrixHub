import { NotificationApiService } from '$lib/services/api/integrations/NotificationApiService';
import { getRequestAbortKind } from '$lib/utils';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { getTestExecutionGate, mapTestExecutionError } from '../../testExecutionGate.svelte';

const TEST_TIMEOUT_MS = 35000;
const NOTIFICATION_DURATION_MS = 4000;

export type PushoverTestResult = {
	ok: boolean;
	configured?: boolean;
	httpCode?: number;
	error?: string;
};

type PushoverTestDeps = {
	api?: Pick<NotificationApiService, 'testPushover'>;
};

export function usePushoverTest(deps: PushoverTestDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const testGate = getTestExecutionGate();
	let testText = $state('Test notification from ESP32');
	let sending = $state(false);
	let lastResult = $state<PushoverTestResult | null>(null);
	let lastError = $state<string | null>(null);

	function createApi() {
		return deps.api ?? apiClient!.createService(NotificationApiService);
	}

	async function sendTest(): Promise<void> {
		if (sending || testGate.isBlocked) return;

		lastError = null;
		lastResult = null;

		const text = testText.trim();
		if (!text) return;

		const finishGate = testGate.tryBeginRequest();
		if (!finishGate) {
			return;
		}

		sending = true;
		const controller = new AbortController();
		const timeoutId = window.setTimeout(() => controller.abort(), TEST_TIMEOUT_MS);

		try {
			const result = await createApi().testPushover(text, controller.signal);
			lastResult = result;

			if (result.ok) {
				notifications.success(
					m.pushover_test_success({ locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
				return;
			}

			const errorMessage =
				mapTestExecutionError(result.error) ?? result.error ?? `HTTP ${result.httpCode ?? '?'}`;
			lastError = errorMessage;
			notifications.error(
				m.error_prefix({ error: errorMessage }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		} catch (error) {
			const abortKind = getRequestAbortKind(error);
			const fallbackMessage = abortKind ? 'Request timed out' : 'Unknown error';
			const message =
				mapTestExecutionError(error) ??
				(error instanceof Error && error.message ? error.message : fallbackMessage);
			lastError = message;
			notifications.error(
				m.error_prefix({ error: message }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
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
