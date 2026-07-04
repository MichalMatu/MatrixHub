import { onDestroy, onMount } from 'svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import {
	SystemApiService,
	type WifiRecoveryResponse
} from '$lib/services/api/core/SystemApiService';
import { Logger } from '$lib/services/core/Logger';
import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import type {
	ExtendedHealthDiagnostics,
	SystemStatusSnapshot
} from '$lib/types/system/systemStatusSnapshot';
import { toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import type { SystemInformation } from '$lib/types/system/system';

// The initial system-status snapshot arrives through the websocket channel, so
// its timeout must outlive the socket handshake/connect timeout to avoid false
// negatives on slow links or cold reconnects.
const SNAPSHOT_TIMEOUT_MS = 15000;

interface SystemStatusManagementDeps {
	createApi?: () => Pick<SystemApiService, 'triggerWifiRecovery'>;
	snapshotTimeoutMs?: number;
}

export function useSystemStatusManagement(deps: SystemStatusManagementDeps = {}) {
	const { createService } = useApiClient();

	let systemInfo = $state<SystemInformation | null>(null);
	let health = $state<ExtendedHealthDiagnostics | null>(null);
	let isApMode = $state(false);
	let loading = $state(true);
	let refreshing = $state(false);
	let error = $state<string | null>(null);
	let snapshotTimeout: ReturnType<typeof setTimeout> | null = null;
	let started = false;

	function getApi() {
		return deps.createApi?.() ?? createService(SystemApiService);
	}

	const systemStatusChannel = createSystemChannelSubscription<SystemStatusSnapshot>({
		channel: 'system_status',
		onSnapshot: applySnapshot,
		onReset: resetSnapshotState
	});

	function resolveApMode(payload: SystemStatusSnapshot) {
		const diagnostics = payload.diagnostics;
		if (diagnostics?.ap?.active) {
			return true;
		}
		if (diagnostics?.wifi?.apActive) {
			return true;
		}
		if (typeof payload.wifi_ap_mode === 'boolean') {
			return payload.wifi_ap_mode;
		}

		return false;
	}

	function clearSnapshotTimeout() {
		if (!snapshotTimeout) return;
		clearTimeout(snapshotTimeout);
		snapshotTimeout = null;
	}

	function finishSnapshotLoadWithError(message: string) {
		error = message;
		loading = false;
		refreshing = false;
		clearSnapshotTimeout();
	}

	function applySnapshot(payload: SystemStatusSnapshot) {
		if (payload.system_info) {
			systemInfo = payload.system_info;
		}
		if (payload.diagnostics) {
			health = payload.diagnostics;
		}
		isApMode = resolveApMode(payload);

		error = null;
		loading = false;
		refreshing = false;
		clearSnapshotTimeout();
	}

	function resetSnapshotState() {
		systemInfo = null;
		health = null;
		isApMode = false;
		finishSnapshotLoadWithError(m.status_error_timeout({ locale: i18n.languageTag }));
	}

	function requestSnapshot() {
		const hasCachedSnapshot = started
			? systemStatusChannel.refresh({ hydrateSnapshot: true })
			: systemStatusChannel.subscribe();

		refreshing = true;
		if (!hasCachedSnapshot && !systemInfo && !health) {
			loading = true;
		}
		error = null;
		clearSnapshotTimeout();
		snapshotTimeout = setTimeout(() => {
			if (!refreshing && !loading) return;
			finishSnapshotLoadWithError(m.status_error_timeout({ locale: i18n.languageTag }));
		}, deps.snapshotTimeoutMs ?? SNAPSHOT_TIMEOUT_MS);

		started = true;
	}

	function fetchAll() {
		requestSnapshot();
	}

	function notifyWifiRecovery(result: WifiRecoveryResponse) {
		// The backend always returns 200 for a handled recovery request, so the UI
		// has to inspect the payload to distinguish "already connected", "queued"
		// and "rejected" instead of treating every successful HTTP response alike.
		if (result.connected) {
			notifications.info(
				m.toast_message(
					{ message: m.status_wifi_recovery_connected({ locale: i18n.languageTag }) },
					{ locale: i18n.languageTag }
				),
				4000
			);
			return;
		}

		if (result.accepted) {
			notifications.success(
				m.toast_message(
					{ message: m.status_wifi_recovery_queued({ locale: i18n.languageTag }) },
					{ locale: i18n.languageTag }
				),
				4000
			);
			return;
		}

		notifications.warning(
			m.toast_message(
				{ message: m.status_wifi_recovery_rejected({ locale: i18n.languageTag }) },
				{ locale: i18n.languageTag }
			),
			5000
		);
	}

	async function triggerWifiRecovery() {
		try {
			const result = await getApi().triggerWifiRecovery();
			notifyWifiRecovery(result);
			requestSnapshot();
		} catch (error) {
			Logger.error('WiFi recovery error:', error);
			const message = toUserRequestErrorMessage(error, {
				timeoutMessage: m.status_wifi_recovery_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.status_wifi_recovery_failed({ locale: i18n.languageTag })
			});
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		}
	}

	onMount(() => {
		requestSnapshot();
	});

	onDestroy(() => {
		clearSnapshotTimeout();
		systemStatusChannel.destroy();
	});

	return {
		get systemInfo() {
			return systemInfo;
		},
		get health() {
			return health;
		},
		get isApMode() {
			return isApMode;
		},
		get loading() {
			return loading;
		},
		get refreshing() {
			return refreshing;
		},
		get error() {
			return error;
		},
		fetchAll,
		triggerWifiRecovery
	};
}
