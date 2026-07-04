import { onMount, onDestroy } from 'svelte';
import { Logger } from '$lib/services/core/Logger';
import { getRequestAbortKind } from './requestErrors';

const POLLING_START_JITTER_MS = 300;

/**
 * Options for the polling composable
 */
export interface PollingOptions {
	/** Automatically start polling on mount (default: true) */
	autoStart?: boolean;
	/** Run an initial poll on mount (default: true) */
	initialPoll?: boolean;
	/** Polling interval in milliseconds (default: 5000) */
	intervalMs?: number;
	/** Delay interval start slightly to avoid synchronized polling bursts (default: true) */
	jitter?: boolean;
	/** Pause polling when tab/window is hidden (default: true) */
	pauseWhenHidden?: boolean;
}

/**
 * Composable for managing periodic polling with automatic lifecycle management.
 *
 * Features:
 * - Automatic setup/teardown with onMount/onDestroy
 * - Prevents overlapping requests with in-flight guard
 * - Optional delayed start to prevent thundering herd
 * - Optional pause when browser tab is hidden
 *
 * @param fetchFn - Async function to call on each poll
 * @param options - Configuration options
 *
 * @example
 * ```typescript
 * const { poll, start, stop, isPolling } = usePolling(
 *   async () => {
 *     data = await api.getData();
 *   },
 *   { intervalMs: 5000, jitter: true }
 * );
 * ```
 */
export function usePolling(fetchFn: () => Promise<void>, options: PollingOptions = {}) {
	const {
		autoStart = true,
		initialPoll = true,
		intervalMs = 5000,
		jitter = true,
		pauseWhenHidden = true
	} = options;

	let pollInterval: ReturnType<typeof setInterval> | undefined = $state(undefined);
	let jitterTimeout: ReturnType<typeof setTimeout> | undefined = undefined;
	let refreshInFlight = $state(false);

	function isPageVisible() {
		return typeof document === 'undefined' || !document.hidden;
	}

	/**
	 * Execute the fetch function with in-flight guard
	 */
	async function poll() {
		// Skip if already fetching or paused
		if (refreshInFlight) {
			return;
		}
		if (pauseWhenHidden && !isPageVisible()) {
			return;
		}

		refreshInFlight = true;
		try {
			await fetchFn();
		} catch (e) {
			const kind = getRequestAbortKind(e);
			if (kind === 'abort') return;
			if (kind === 'timeout') {
				Logger.warn('Polling request timed out:', e);
				return;
			}
			Logger.error('Polling error:', e);
		} finally {
			refreshInFlight = false;
		}
	}

	/**
	 * Start the polling interval
	 */
	function start() {
		if (pollInterval || jitterTimeout) {
			return; // Already started
		}

		const delay = jitter ? POLLING_START_JITTER_MS : 0;

		jitterTimeout = setTimeout(() => {
			jitterTimeout = undefined;
			pollInterval = setInterval(poll, intervalMs);
		}, delay);
	}

	/**
	 * Stop the polling interval
	 */
	function stop() {
		if (jitterTimeout) {
			clearTimeout(jitterTimeout);
			jitterTimeout = undefined;
		}
		if (pollInterval) {
			clearInterval(pollInterval);
			pollInterval = undefined;
		}
	}

	/**
	 * Handle visibility change (if pauseWhenHidden enabled)
	 */
	function handleVisibilityChange() {
		// Resume polling immediately when tab becomes visible
		if (isPageVisible() && pollInterval) {
			poll();
		}
	}

	// Lifecycle: auto-start on mount, cleanup on destroy
	onMount(async () => {
		// Setup visibility listener if needed
		if (pauseWhenHidden) {
			document.addEventListener('visibilitychange', handleVisibilityChange);
		}

		if (!autoStart) return;

		// Initial fetch
		if (initialPoll) {
			await poll();
		}

		// Start polling
		start();
	});

	onDestroy(() => {
		stop();
		if (pauseWhenHidden) {
			document.removeEventListener('visibilitychange', handleVisibilityChange);
		}
	});

	return {
		poll,
		start,
		stop,
		/** Whether polling is currently active */
		get isPolling() {
			return pollInterval !== undefined || jitterTimeout !== undefined;
		},
		/** Whether a fetch is currently in progress */
		get isFetching() {
			return refreshInFlight;
		}
	};
}
