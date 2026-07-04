import { onDestroy } from 'svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { Logger } from '$lib/services/core/Logger';
import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import type { SystemStatusSnapshot } from '$lib/types/system/systemStatusSnapshot';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';

const SNAPSHOT_TIMEOUT_MS = 5000;

interface SnapshotBackedStatusOptions<TStatus extends Record<string, unknown>> {
	defaultStatus: TStatus;
	mapSnapshot: (snapshot: SystemStatusSnapshot) => TStatus | null;
	errorCooldownMs: number;
	fallbackMessage: () => string;
	loggerLabel: string;
}

interface PendingSnapshotRequest {
	resolve: () => void;
	timeoutId: ReturnType<typeof setTimeout>;
}

function createSnapshotTimeoutError() {
	return new Error('system status snapshot timed out');
}

export function createSystemStatusBackedStatus<TStatus extends Record<string, unknown>>({
	defaultStatus,
	mapSnapshot,
	errorCooldownMs,
	fallbackMessage,
	loggerLabel
}: SnapshotBackedStatusOptions<TStatus>) {
	// Wi-Fi/AP screens no longer poll their own status endpoint by default.
	// Instead they derive page-specific status objects from the shared
	// "system_status" snapshot:
	// backend JSON snapshot -> systemStatus store cache/event bus -> mapSnapshot()
	// -> typed status used by the screen component.
	let status = $state<TStatus>({ ...defaultStatus });
	let error = $state<string | null>(null);
	let lastErrorAt = $state(0);
	let lastErrorMsg = $state<string | null>(null);
	let started = false;

	const pendingSnapshotRequests = new Set<PendingSnapshotRequest>();
	const systemStatusChannel = createSystemChannelSubscription<SystemStatusSnapshot>({
		channel: 'system_status',
		onSnapshot: applySnapshot,
		onReset: resetStatus
	});

	function setStatusError(nextError: unknown, fallback: string) {
		const abortKind = getRequestAbortKind(nextError);
		if (abortKind === 'abort') return;

		const message = toUserRequestErrorMessage(nextError, {
			fallbackMessage: fallback,
			timeoutMessage: m.toast_request_timeout({ locale: i18n.languageTag })
		});
		const now = Date.now();
		if (message === lastErrorMsg && now - lastErrorAt < errorCooldownMs) return;

		lastErrorAt = now;
		lastErrorMsg = message;
		error = message;
		notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
	}

	function resolvePendingSnapshotRequests() {
		pendingSnapshotRequests.forEach((request) => {
			clearTimeout(request.timeoutId);
			request.resolve();
		});
		pendingSnapshotRequests.clear();
	}

	function applySnapshot(snapshot: SystemStatusSnapshot) {
		const nextStatus = mapSnapshot(snapshot);
		if (!nextStatus) return;
		status = nextStatus;
		error = null;
		resolvePendingSnapshotRequests();
	}

	function resetStatus() {
		status = { ...defaultStatus };
		error = null;
	}

	function createSnapshotWait() {
		let request: PendingSnapshotRequest | null = null;
		const promise = new Promise<void>((resolve) => {
			request = {} as PendingSnapshotRequest;
			request.resolve = () => {
				if (!request || !pendingSnapshotRequests.delete(request)) return;
				clearTimeout(request.timeoutId);
				resolve();
			};
			request.timeoutId = setTimeout(() => {
				if (!request || !pendingSnapshotRequests.delete(request)) return;
				// No snapshot reached this feature hook in time. We keep the previous
				// cached/default status on screen and surface an error instead of
				// clearing the widget to "empty", which would look like a second bug.
				Logger.error(`${loggerLabel} snapshot timeout`);
				setStatusError(createSnapshotTimeoutError(), fallbackMessage());
				resolve();
			}, SNAPSHOT_TIMEOUT_MS);
			pendingSnapshotRequests.add(request);
		});

		return {
			promise,
			cancel() {
				request?.resolve();
			}
		};
	}

	async function waitForLiveSnapshot(mode: 'start' | 'refresh') {
		// Register the waiter before subscribe/refresh so a very fast live snapshot
		// cannot arrive in the tiny gap between "request snapshot" and "start waiting".
		const pendingSnapshot = createSnapshotWait();
		const hasCachedSnapshot =
			mode === 'start'
				? systemStatusChannel.subscribe()
				: systemStatusChannel.refresh({ hydrateSnapshot: true });
		if (hasCachedSnapshot) {
			pendingSnapshot.cancel();
			return;
		}
		await pendingSnapshot.promise;
	}

	async function start() {
		if (started) {
			await refresh();
			return;
		}

		started = true;
		await waitForLiveSnapshot('start');
	}

	async function refresh() {
		if (!started) {
			started = true;
			await waitForLiveSnapshot('start');
			return;
		}

		await waitForLiveSnapshot('refresh');
	}

	onDestroy(() => {
		resolvePendingSnapshotRequests();
		systemStatusChannel.destroy();
	});

	return {
		get status() {
			return status;
		},
		get error() {
			return error;
		},
		start,
		refresh
	};
}
