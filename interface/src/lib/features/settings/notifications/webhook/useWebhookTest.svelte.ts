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
const DEFAULT_GENERIC_PAYLOAD = '{"event":"test","message":"Hello from ESP32"}';
const DEFAULT_DISCORD_PAYLOAD = '{"content":"Hello from ESP32"}';

type WebhookTestResult = {
	ok: boolean;
	configured?: boolean;
	httpCode?: number;
	error?: string;
	response?: string;
};

type WebhookTestDeps = {
	api?: Pick<NotificationApiService, 'testWebhook'>;
};

export function useWebhookTest(getWebhookUrl: () => string = () => '', deps: WebhookTestDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const testGate = getTestExecutionGate();
	let testText = $state(DEFAULT_GENERIC_PAYLOAD);
	let sending = $state(false);
	let lastResult = $state<WebhookTestResult | null>(null);
	let lastError = $state<string | null>(null);
	let userEdited = $state(false);
	let lastAutoMode = $state<'discord' | 'generic' | null>(null);

	const webhookUrl = $derived(getWebhookUrl());
	const isDiscordUrl = $derived(isDiscordWebhookUrl(webhookUrl));

	function isDiscordWebhookUrl(url: string): boolean {
		return /discord(?:app)?\.com\/api\/webhooks/i.test(url);
	}

	function createApi() {
		return deps.api ?? apiClient!.createService(NotificationApiService);
	}

	$effect(() => {
		const mode: 'discord' | 'generic' = isDiscordUrl ? 'discord' : 'generic';
		const shouldAutoSwap =
			!userEdited || testText === DEFAULT_GENERIC_PAYLOAD || testText === DEFAULT_DISCORD_PAYLOAD;
		if (shouldAutoSwap && mode !== lastAutoMode) {
			testText = mode === 'discord' ? DEFAULT_DISCORD_PAYLOAD : DEFAULT_GENERIC_PAYLOAD;
			lastAutoMode = mode;
		}
	});

	async function sendTest(): Promise<void> {
		if (sending || testGate.isBlocked) return;

		lastError = null;
		lastResult = null;

		const text = testText.trim();

		const finishGate = testGate.tryBeginRequest();
		if (!finishGate) {
			return;
		}

		sending = true;
		const controller = new AbortController();
		const timeoutId = window.setTimeout(() => controller.abort(), TEST_TIMEOUT_MS);

		try {
			const res = await createApi().testWebhook(text, controller.signal);
			lastResult = res;

			if (res.ok) {
				notifications.success(
					m.toast_webhook_test_sent({ locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
			} else if (res.configured === false) {
				notifications.error(
					m.toast_webhook_not_configured({ locale: i18n.languageTag }),
					ERROR_DURATION_MS
				);
			} else {
				const errorMessage =
					mapTestExecutionError(res.error) ??
					res.error ??
					m.toast_webhook_test_failed({ locale: i18n.languageTag });
				notifications.error(
					m.toast_message({ message: errorMessage }, { locale: i18n.languageTag }),
					LONG_ERROR_DURATION_MS
				);
			}
		} catch (e) {
			const msg = e instanceof Error ? e.message : String(e);
			const abortKind = getRequestAbortKind(e);
			if (abortKind) {
				lastError = m.toast_webhook_test_timeout({ locale: i18n.languageTag });
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
			userEdited = true;
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
