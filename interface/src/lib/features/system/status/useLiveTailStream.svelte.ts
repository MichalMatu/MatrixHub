import type { SystemApiService } from '$lib/services/api/core/SystemApiService';
import { Logger } from '$lib/services/core/Logger';
import { usePolling } from '$lib/utils/api/usePolling.svelte';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

export interface LogEntry {
	id: number;
	timestampMs: number;
	level: string;
	tag: string;
	message: string;
}

const TAIL_REFRESH_INTERVAL_MS = 10000;
// Keep the admin tail useful without asking the firmware for a full ring dump on
// every refresh. 128 lines keeps the request compact while still giving enough
// context for debugging recent failures.
const TAIL_REQUEST_MAX_LINES = 128;
const TAIL_ERROR_THROTTLE_MS = 15000;
const START_DELAY_MS = 800;

export function useLiveTailStream(getApi: () => SystemApiService) {
	let tail = $state<LogEntry[]>([]);
	let error = $state('');
	let capacity = $state<number | undefined>(undefined);
	let isPaused = $state(false);

	let lastTailErrorAtMs = 0;
	let lastClearedTimestampMs = 0;
	let nextId = 1;
	let startTimer: ReturnType<typeof setTimeout> | null = null;

	const poller = usePolling(() => refreshTail(), {
		autoStart: false,
		initialPoll: false,
		pauseWhenHidden: true,
		intervalMs: TAIL_REFRESH_INTERVAL_MS,
		jitter: true
	});

	function start() {
		if (startTimer) {
			clearTimeout(startTimer);
			startTimer = null;
		}

		if (isPaused) return;

		startTimer = setTimeout(() => {
			startTimer = null;
			if (isPaused) return;
			void refreshTail();
			poller.start();
		}, START_DELAY_MS);
	}

	function stop() {
		if (startTimer) {
			clearTimeout(startTimer);
			startTimer = null;
		}
		poller.stop();
	}

	function togglePause() {
		isPaused = !isPaused;
		if (isPaused) {
			poller.stop();
			return;
		}

		void refreshTail();
		poller.start();
	}

	function resetLocalTail() {
		tail = [];
		error = '';
		lastClearedTimestampMs = 0;
		nextId = 1;
	}

	async function clear() {
		try {
			await getApi().clearLogTail();
			resetLocalTail();
		} catch (nextError) {
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			Logger.error('Failed to clear tail logs:', nextError);
			setTailErrorThrottled(
				toUserRequestErrorMessage(nextError, {
					fallbackMessage: m.livetail_error_clear({ locale: i18n.languageTag })
				})
			);
		}
	}

	async function copyToClipboard() {
		if (tail.length === 0) return;
		const text = tail.map((line) => `[${line.level}] ${line.tag}: ${line.message}`).join('\n');
		try {
			await navigator.clipboard.writeText(text);
		} catch (nextError) {
			Logger.error('Failed to copy tail logs:', nextError);
		}
	}

	async function refreshTail() {
		try {
			const lastLogTimestamp =
				tail.length > 0 ? Math.max(...tail.map((line) => line.timestampMs)) : 0;
			const since = Math.max(lastLogTimestamp, lastClearedTimestampMs);
			const data = await getApi().getLogTail(TAIL_REQUEST_MAX_LINES, since);
			const newSourceLines = data.lines ?? [];

			if (newSourceLines.length > 0) {
				const newEntries: LogEntry[] = newSourceLines.map((line) => ({
					id: nextId++,
					timestampMs: line.t,
					level: line.l,
					tag: line.g,
					message: line.m
				}));

				const filteredEntries = newEntries.filter(
					(nextLine) =>
						!tail.some(
							(existingLine) =>
								existingLine.timestampMs === nextLine.timestampMs &&
								existingLine.tag === nextLine.tag &&
								existingLine.message === nextLine.message
						)
				);

				if (filteredEntries.length > 0) {
					tail = [...tail, ...filteredEntries].slice(-TAIL_REQUEST_MAX_LINES);
				}
			}

			if (data.capacity !== undefined) {
				capacity = data.capacity;
			}
			error = '';
			lastTailErrorAtMs = 0;
		} catch (nextError) {
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			if (kind === 'timeout') {
				setTailErrorThrottled(m.livetail_error_tail_timeout({ locale: i18n.languageTag }));
				return;
			}

			Logger.error('Tail fetch error:', nextError);
			setTailErrorThrottled(
				toUserRequestErrorMessage(nextError, {
					fallbackMessage: m.livetail_error_tail_fallback({ locale: i18n.languageTag })
				})
			);
		}
	}

	function setTailErrorThrottled(message: string) {
		const now = Date.now();
		const changed = error !== message;
		const stale = now - lastTailErrorAtMs >= TAIL_ERROR_THROTTLE_MS;
		if (changed || stale) {
			error = message;
			lastTailErrorAtMs = now;
		}
	}

	return {
		get tail() {
			return tail;
		},
		get error() {
			return error;
		},
		get capacity() {
			return capacity;
		},
		get isPaused() {
			return isPaused;
		},
		start,
		stop,
		togglePause,
		clear,
		copyToClipboard,
		refreshTail
	};
}
