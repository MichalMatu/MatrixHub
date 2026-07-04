import { PowerApiService } from '$lib/services/api/core/PowerApiService';
import { getRequestAbortKind, getRequestFailureKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import * as m from '$lib/paraglide/messages.js';

export type RestartStage = 'saving' | 'restarting' | 'waiting' | 'success' | 'error';

type ProbeResult =
	| { ok: true; uptimeSeconds?: number }
	| { ok: false; status?: number; error?: unknown };

export interface RestartSequenceOptions {
	// Returning `false` aborts the restart flow before any restart is triggered.
	onSave: () => Promise<unknown>;
	triggerRestart?: () => Promise<void>;
	getTriggerRestart?: () => (() => Promise<void>) | undefined;
	useSleepInsteadOfRestart?: boolean | (() => boolean);
}

interface RestartPowerApi {
	getStatus(options?: { timeoutMs?: number }): Promise<{ uptime_ms?: number }>;
	restart(): Promise<void>;
	requestHygieneSleepWithTimeout(timeoutMs?: number): Promise<void>;
}

interface RestartSequenceDeps {
	createPowerApi?: () => RestartPowerApi;
	reload?: () => void;
}

const HEALTH_POLL = {
	intervalMs: 500,
	initialDelayMs: 500,
	maxWaitMs: 15000,
	requestTimeoutMs: 1500,
	reloadDelayMs: 300
} as const;

function createAbortError() {
	return new DOMException('Restart sequence aborted', 'AbortError');
}

export function useRestartSequence(
	options: RestartSequenceOptions,
	deps: RestartSequenceDeps = {}
) {
	const apiClient = useApiClient();
	let stage = $state<RestartStage>('saving');
	let errorMessage = $state('');
	let elapsedSeconds = $state(0);
	let seenOffline = $state(false);

	let intervalId: ReturnType<typeof setInterval> | null = null;
	let reloadTimer: ReturnType<typeof setTimeout> | null = null;
	let activeRunId = 0;
	let disposed = false;
	let runningPromise: Promise<void> | null = null;
	const pendingSleeps = new Map<ReturnType<typeof setTimeout>, () => void>();

	function createPowerApi(): RestartPowerApi {
		return deps.createPowerApi?.() ?? apiClient.createService(PowerApiService);
	}

	function isRunActive(runId: number) {
		return !disposed && activeRunId === runId;
	}

	function throwIfInactive(runId: number) {
		if (!isRunActive(runId)) {
			throw createAbortError();
		}
	}

	function startTimer() {
		stopTimer();
		elapsedSeconds = 0;
		intervalId = setInterval(() => {
			elapsedSeconds += 1;
		}, 1000);
	}

	function stopTimer() {
		if (!intervalId) return;
		clearInterval(intervalId);
		intervalId = null;
	}

	function clearReloadTimer() {
		if (!reloadTimer) return;
		clearTimeout(reloadTimer);
		reloadTimer = null;
	}

	function clearPendingSleeps() {
		for (const [timeoutId, resolve] of pendingSleeps) {
			clearTimeout(timeoutId);
			resolve();
		}
		pendingSleeps.clear();
	}

	async function sleep(ms: number, runId: number): Promise<void> {
		await new Promise<void>((resolve) => {
			const timeoutId = setTimeout(() => {
				pendingSleeps.delete(timeoutId);
				resolve();
			}, ms);
			pendingSleeps.set(timeoutId, resolve);
		});
		throwIfInactive(runId);
	}

	function getUseSleep() {
		return typeof options.useSleepInsteadOfRestart === 'function'
			? options.useSleepInsteadOfRestart()
			: options.useSleepInsteadOfRestart;
	}

	function getTriggerRestart() {
		return options.getTriggerRestart ? options.getTriggerRestart() : options.triggerRestart;
	}

	function shouldIgnoreRestartTriggerFailure(error: unknown) {
		if (!getUseSleep()) return false;
		const kind = getRequestFailureKind(error);
		return kind === 'abort' || kind === 'timeout' || kind === 'network';
	}

	async function captureBaselineUptimeMs(runId: number): Promise<number | null> {
		if (!getUseSleep()) {
			return null;
		}

		throwIfInactive(runId);

		try {
			const body = await createPowerApi().getStatus({
				timeoutMs: HEALTH_POLL.requestTimeoutMs
			});
			throwIfInactive(runId);

			return typeof body.uptime_ms === 'number' ? body.uptime_ms : null;
		} catch (error) {
			if (getRequestAbortKind(error) === 'abort') throw error;
			return null;
		}
	}

	function hasObservedReboot(
		currentUptimeSeconds: number | undefined,
		baselineUptimeMs: number | null
	): boolean {
		if (typeof currentUptimeSeconds !== 'number' || baselineUptimeMs === null) {
			return false;
		}

		const currentUptimeMs = currentUptimeSeconds * 1000;
		return currentUptimeMs + 250 < baselineUptimeMs;
	}

	async function probeHealth(runId: number): Promise<ProbeResult> {
		throwIfInactive(runId);
		try {
			const body = await createPowerApi().getStatus({
				timeoutMs: HEALTH_POLL.requestTimeoutMs
			});
			throwIfInactive(runId);

			const uptimeSeconds = typeof body.uptime_ms === 'number' ? body.uptime_ms / 1000 : undefined;
			return { ok: true, uptimeSeconds };
		} catch (error) {
			if (getRequestAbortKind(error) === 'abort') throw error;
			return { ok: false, error };
		}
	}

	async function waitForDeviceBackOnline(
		runId: number,
		baselineUptimeMs: number | null
	): Promise<void> {
		seenOffline = false;
		await sleep(HEALTH_POLL.initialDelayMs, runId);
		const deadline = Date.now() + HEALTH_POLL.maxWaitMs;

		while (Date.now() < deadline) {
			throwIfInactive(runId);
			const result = await probeHealth(runId);

			if (result.ok) {
				if (seenOffline) return;

				const useSleep = getUseSleep();
				if (useSleep && hasObservedReboot(result.uptimeSeconds, baselineUptimeMs)) {
					return;
				}
				if (typeof result.uptimeSeconds === 'number' && result.uptimeSeconds <= 15 && !useSleep) {
					return;
				}
				if (elapsedSeconds >= 6 && !useSleep) return;
			} else {
				seenOffline = true;
			}

			await sleep(HEALTH_POLL.intervalMs, runId);
		}

		throw new Error('Timed out waiting for device to come back online');
	}

	function scheduleReload(runId: number) {
		clearReloadTimer();
		reloadTimer = setTimeout(() => {
			reloadTimer = null;
			if (!isRunActive(runId)) return;
			(deps.reload ?? (() => window.location.reload()))();
		}, HEALTH_POLL.reloadDelayMs);
	}

	async function runRestartSequence() {
		if (runningPromise) return runningPromise;

		disposed = false;
		errorMessage = '';
		const runId = ++activeRunId;

		runningPromise = (async () => {
			try {
				stage = 'saving';
				const saveResult = await options.onSave();
				if (saveResult === false) {
					throw new Error('Save step did not complete successfully');
				}
				throwIfInactive(runId);
				const baselineUptimeMs = await captureBaselineUptimeMs(runId);

				stage = 'restarting';
				const triggerRestart = getTriggerRestart();
				try {
					if (triggerRestart) {
						await triggerRestart();
					} else {
						const powerApi = createPowerApi();
						if (getUseSleep()) {
							await powerApi.requestHygieneSleepWithTimeout();
						} else {
							await powerApi.restart();
						}
					}
				} catch (error) {
					if (!shouldIgnoreRestartTriggerFailure(error)) {
						throw error;
					}
				}
				throwIfInactive(runId);

				stage = 'waiting';
				startTimer();
				await waitForDeviceBackOnline(runId, baselineUptimeMs);
				throwIfInactive(runId);

				stopTimer();
				stage = 'success';
				scheduleReload(runId);
			} catch (error) {
				stopTimer();
				if (!isRunActive(runId) && getRequestAbortKind(error) === 'abort') return;
				stage = 'error';
				const kind = getRequestAbortKind(error);
				if (kind === 'abort') {
					errorMessage = m.restart_error_cancelled();
					return;
				}

				errorMessage = toUserRequestErrorMessage(error, {
					timeoutMessage: m.restart_error_timeout(),
					fallbackMessage: m.restart_error_fallback()
				});
			} finally {
				if (activeRunId === runId) {
					runningPromise = null;
				}
			}
		})();

		return runningPromise;
	}

	function destroy() {
		disposed = true;
		activeRunId += 1;
		stopTimer();
		clearReloadTimer();
		clearPendingSleeps();
	}

	return {
		get stage() {
			return stage;
		},
		get errorMessage() {
			return errorMessage;
		},
		get elapsedSeconds() {
			return elapsedSeconds;
		},
		get seenOffline() {
			return seenOffline;
		},
		runRestartSequence,
		destroy
	};
}
