import { notifications } from '$lib/components/toasts/notifications.svelte';
import { Logger } from '$lib/services/core/Logger';
import { UdpApiService } from '$lib/services/api/integrations/UdpApiService';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { getTestExecutionGate, mapTestExecutionError } from '../../testExecutionGate.svelte';

const NOTIFICATION_DURATION_MS = 4000;

type UdpTestDeps = {
	api?: Pick<UdpApiService, 'testSend'>;
	notifications?: Pick<typeof notifications, 'success' | 'error'>;
	logger?: Pick<typeof Logger, 'error'>;
};

type UdpTarget = {
	host: string;
	port: number;
};

export function useUdpTest(getTarget: () => UdpTarget, deps: UdpTestDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const testGate = getTestExecutionGate();
	const toast = deps.notifications ?? notifications;
	const logger = deps.logger ?? Logger;

	function createApi() {
		return deps.api ?? apiClient!.createService(UdpApiService);
	}

	let sending = $state(false);
	let lastError = $state<string | null>(null);

	async function sendTest(): Promise<void> {
		if (sending || testGate.isBlocked) return;

		lastError = null;

		const finishGate = testGate.tryBeginRequest();
		if (!finishGate) {
			return;
		}

		sending = true;

		try {
			const result = await createApi().testSend();
			if (result.success) {
				const { host, port } = getTarget();
				toast.success(
					m.udp_test_success({ host, port: String(port) }, { locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
				return;
			}

			const message = result.message || m.toast_test_failed({ locale: i18n.languageTag });
			lastError = message;
			toast.error(
				m.toast_message({ message }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		} catch (cause) {
			logger.error('UDP test failed:', cause);
			const testErrorMessage = mapTestExecutionError(cause);
			if (testErrorMessage) {
				lastError = testErrorMessage;
				toast.error(
					m.toast_message({ message: lastError }, { locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
				return;
			}
			if (getRequestAbortKind(cause) === 'abort') return;

			const message = toUserRequestErrorMessage(cause, {
				fallbackMessage: m.toast_test_failed({ locale: i18n.languageTag })
			});
			lastError = message;
			toast.error(
				m.toast_message({ message }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		} finally {
			sending = false;
			finishGate();
		}
	}

	return {
		get sending() {
			return sending;
		},
		get blocked() {
			return sending || testGate.isBlocked;
		},
		get lastError() {
			return lastError;
		},
		sendTest
	};
}
