import { systemStatus } from '../systemStatus.svelte';
import type { SystemEvent } from './types';

interface SystemChannelLeaseStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
}

interface SystemStatusChannelStore extends SystemChannelLeaseStore {
	getSnapshot<TSnapshot>(channel: string): TSnapshot | null;
	requestSnapshot?(channel: string): void;
	subscribeEvents?(run: (value: SystemEvent | null) => void): () => void;
}

interface SystemEventsBus {
	subscribe(run: (value: SystemEvent | null) => void): () => void;
}

interface SystemChannelSubscriptionOptions<TSnapshot> {
	channel: string;
	onSnapshot?: (snapshot: TSnapshot) => void;
	onEvent?: (event: SystemEvent) => void;
	onReset?: () => void;
	systemStatusStore?: SystemStatusChannelStore;
	systemEventsBus?: SystemEventsBus;
}

interface SubscribeOptions {
	hydrateSnapshot?: boolean;
}

export function createSystemChannelSubscription<TSnapshot>({
	channel,
	onSnapshot,
	onEvent,
	onReset,
	systemStatusStore = systemStatus,
	systemEventsBus
}: SystemChannelSubscriptionOptions<TSnapshot>) {
	// This helper is the thin adapter between feature hooks and the global
	// systemStatus store:
	// - it can hydrate from the last cached snapshot
	// - it can acquire/release a backend channel lease
	// - it forwards matching events/snapshots to one feature-specific callback
	let unsubscribeEvents: (() => void) | null = null;
	let subscribed = false;
	let managesChannel = false;

	function handleEvent(event: SystemEvent | null) {
		if (!subscribed) return;
		if (!event) {
			if (getSnapshot() === null) {
				onReset?.();
			}
			return;
		}

		if (event.type === 'snapshot') {
			if (event.channel === channel) {
				onSnapshot?.(event.data as TSnapshot);
			}
			return;
		}

		onEvent?.(event);
	}

	function ensureEventSubscription() {
		if (unsubscribeEvents) return;
		if (systemEventsBus) {
			unsubscribeEvents = systemEventsBus.subscribe(handleEvent);
			return;
		}

		unsubscribeEvents = systemStatusStore.subscribeEvents?.(handleEvent) ?? null;
	}

	function getSnapshot() {
		return systemStatusStore.getSnapshot<TSnapshot>(channel);
	}

	function hydrateSnapshot() {
		const snapshot = getSnapshot();
		if (snapshot === null) return false;
		// Hydration is synchronous and uses only the frontend cache. No websocket
		// request is sent here; that happens later when subscribe()/refresh() asks
		// the store for a live backend snapshot.
		onSnapshot?.(snapshot);
		return true;
	}

	function subscribe(options: SubscribeOptions = {}) {
		const { hydrateSnapshot: shouldHydrateSnapshot = true } = options;

		ensureEventSubscription();
		const hasCachedSnapshot = shouldHydrateSnapshot ? hydrateSnapshot() : false;
		// Mark the subscription as live before asking the transport for a fresh snapshot.
		// Otherwise an already-open socket can deliver the snapshot synchronously and we
		// drop it, forcing widgets to wait for their slower HTTP fallback path.
		subscribed = true;
		if (!managesChannel) {
			// The subscription object may outlive components during reconnects, so
			// channel ownership is tracked explicitly instead of assuming 1:1 wiring.
			systemStatusStore.subscribeChannel(channel);
			managesChannel = true;
		}
		return hasCachedSnapshot;
	}

	function refresh(options: SubscribeOptions = {}) {
		const { hydrateSnapshot: shouldHydrateSnapshot = false } = options;
		ensureEventSubscription();
		const hasCachedSnapshot = shouldHydrateSnapshot ? hydrateSnapshot() : false;
		subscribed = true;
		// Explicit refresh asks the backend to rebuild the current channel snapshot
		// even if we already have a cached copy locally.
		systemStatusStore.requestSnapshot?.(channel);
		return hasCachedSnapshot;
	}

	function unsubscribe() {
		if (!subscribed) return;
		if (managesChannel) {
			systemStatusStore.unsubscribeChannel(channel);
			managesChannel = false;
		}
		subscribed = false;
	}

	function destroy() {
		unsubscribe();
		unsubscribeEvents?.();
		unsubscribeEvents = null;
	}

	return {
		subscribe,
		refresh,
		unsubscribe,
		destroy
	};
}
