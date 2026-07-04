import type { MacroApiService, ScriptStatus } from '$lib/services/api/integrations/MacroApiService';
import { useMacroStatusReadModel } from './useMacroStatusReadModel.svelte';
import { useSystemTransportManagement } from './useSystemTransportManagement.svelte';

type MacroStatusApi = Pick<MacroApiService, 'getStatus'>;
// Route transitions can briefly destroy one consumer before the next one mounts.
// Without a grace period that caused visible WS churn for "macros":
// unsubscribe -> subscribe a moment later. We intentionally keep the lease warm
// for a short time so quick navigation looks like one continuous observer.
const MACROS_CHANNEL_RELEASE_DELAY_MS = 250;
const defaultChannelStore = useSystemTransportManagement();

interface MacroChannelStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
}

interface MacroStatusReadModelLike {
	readonly status: ScriptStatus | null;
	setStatus(status: ScriptStatus | null): void;
}

interface MacroStatusSyncDeps {
	createApi: () => MacroStatusApi;
	channelStore?: MacroChannelStore;
	readModel?: MacroStatusReadModelLike;
	onError?: (error: unknown) => void;
}

interface SharedMacroChannelState {
	activeConsumers: number;
	releaseTimer: ReturnType<typeof setTimeout> | null;
	subscribed: boolean;
	channelStore: MacroChannelStore;
}

const sharedMacroChannels = new WeakMap<object, SharedMacroChannelState>();

function getSharedMacroChannelState(channelStore?: MacroChannelStore) {
	const store = (channelStore ?? defaultChannelStore) as MacroChannelStore & object;
	let state = sharedMacroChannels.get(store);

	if (!state) {
		state = {
			activeConsumers: 0,
			releaseTimer: null,
			subscribed: false,
			channelStore: channelStore ?? defaultChannelStore
		};
		sharedMacroChannels.set(store, state);
	}

	return state;
}

function acquireSharedMacrosChannel(channelStore?: MacroChannelStore) {
	const state = getSharedMacroChannelState(channelStore);
	if (state.releaseTimer) {
		clearTimeout(state.releaseTimer);
		state.releaseTimer = null;
	}

	state.activeConsumers += 1;
	if (!state.subscribed) {
		// First consumer acquires the real /ws/system channel lease.
		state.channelStore.subscribeChannel('macros');
		state.subscribed = true;
	}
}

function releaseSharedMacrosChannel(channelStore?: MacroChannelStore) {
	const state = getSharedMacroChannelState(channelStore);
	if (state.activeConsumers === 0) {
		return;
	}

	state.activeConsumers -= 1;
	if (state.activeConsumers > 0 || state.releaseTimer) {
		return;
	}

	state.releaseTimer = setTimeout(() => {
		state.releaseTimer = null;
		if (state.activeConsumers === 0) {
			// Only after the grace window do we release the backend subscription.
			// If logs show unsubscribe/subscribe pairs again during fast navigation,
			// start debugging here first.
			state.channelStore.unsubscribeChannel('macros');
			state.subscribed = false;
		}
	}, MACROS_CHANNEL_RELEASE_DELAY_MS);
}

export function useMacroStatusSync(deps: MacroStatusSyncDeps) {
	const macroStatus = deps.readModel ?? useMacroStatusReadModel();

	let statusAbort: AbortController | null = null;
	let disposed = false;
	let channelAcquired = false;

	function isAbortError(error: unknown) {
		return error instanceof DOMException && error.name === 'AbortError';
	}

	function getStatus() {
		return macroStatus.status;
	}

	async function refreshStatus() {
		statusAbort?.abort();
		const controller = new AbortController();
		statusAbort = controller;

		try {
			const status = await deps.createApi().getStatus(controller.signal);
			if (disposed || controller.signal.aborted) return;
			macroStatus.setStatus(status);
		} catch (error) {
			if (controller.signal.aborted || isAbortError(error)) return;
			deps.onError?.(error);
		} finally {
			if (statusAbort === controller) {
				statusAbort = null;
			}
		}
	}

	async function init() {
		disposed = false;
		if (!channelAcquired) {
			acquireSharedMacrosChannel(deps.channelStore);
			channelAcquired = true;
		}

		await refreshStatus();
	}

	function destroy() {
		disposed = true;
		statusAbort?.abort();
		statusAbort = null;

		if (channelAcquired) {
			releaseSharedMacrosChannel(deps.channelStore);
			channelAcquired = false;
		}
	}

	return {
		get status() {
			return getStatus();
		},
		refreshStatus,
		init,
		destroy
	};
}
