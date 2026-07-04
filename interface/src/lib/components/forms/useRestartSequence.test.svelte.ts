import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useRestartSequence } from './useRestartSequence.svelte';

vi.mock('$lib/paraglide/messages.js', () => ({
	restart_error_cancelled: () => 'cancelled',
	restart_error_timeout: () => 'timeout',
	restart_error_fallback: () => 'failed',
	request_error_network: () => 'network error',
	request_error_failed: () => 'request failed',
	toast_request_timeout: () => 'request timeout'
}));

function flushMicrotasks() {
	return new Promise<void>((resolve) => queueMicrotask(resolve));
}

function createDeferred<T>() {
	let resolve!: (value: T | PromiseLike<T>) => void;
	let reject!: (reason?: unknown) => void;
	const promise = new Promise<T>((res, rej) => {
		resolve = res;
		reject = rej;
	});
	return { promise, resolve, reject };
}

describe('useRestartSequence', () => {
	beforeEach(() => {
		vi.useFakeTimers();
	});

	afterEach(() => {
		vi.useRealTimers();
	});

	it('does not continue restart flow after destroy during save', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const saveDeferred = createDeferred<void>();
				const restart = vi.fn();

				const sequence = useRestartSequence(
					{
						onSave: () => saveDeferred.promise
					},
					{
						createPowerApi: () => ({
							getStatus: vi.fn(),
							restart,
							requestHygieneSleepWithTimeout: vi.fn()
						})
					}
				);

				const runPromise = sequence.runRestartSequence();
				sequence.destroy();
				saveDeferred.resolve();

				void runPromise.then(async () => {
					await flushMicrotasks();
					expect(restart).not.toHaveBeenCalled();
					expect(sequence.stage).toBe('saving');
					expect(sequence.errorMessage).toBe('');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('does not restart when the save step resolves to false', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const restart = vi.fn();
				const getStatus = vi.fn();
				const sequence = useRestartSequence(
					{
						onSave: vi.fn().mockResolvedValue(false)
					},
					{
						createPowerApi: () => ({
							getStatus,
							restart,
							requestHygieneSleepWithTimeout: vi.fn()
						})
					}
				);

				void sequence.runRestartSequence().then(async () => {
					await flushMicrotasks();
					expect(restart).not.toHaveBeenCalled();
					expect(getStatus).not.toHaveBeenCalled();
					expect(sequence.stage).toBe('error');
					expect(sequence.errorMessage).toBe('Save step did not complete successfully');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('clears pending reload when destroyed after successful restart', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const reload = vi.fn();
				const getStatus = vi
					.fn()
					.mockRejectedValueOnce(new Error('offline'))
					.mockResolvedValueOnce({ uptime_ms: 5000 });

				const sequence = useRestartSequence(
					{
						onSave: vi.fn().mockResolvedValue(undefined)
					},
					{
						reload,
						createPowerApi: () => ({
							getStatus,
							restart: vi.fn().mockResolvedValue(undefined),
							requestHygieneSleepWithTimeout: vi.fn().mockResolvedValue(undefined)
						})
					}
				);

				void sequence.runRestartSequence().then(async () => {
					expect(sequence.stage).toBe('success');
					sequence.destroy();
					vi.advanceTimersByTime(500);
					await flushMicrotasks();
					expect(reload).not.toHaveBeenCalled();
					resolve();
				});

				void vi
					.runAllTimersAsync()
					.then(async () => {
						await flushMicrotasks();
					})
					.catch((error) => {
						throw error;
					});
			});
		});

		cleanup?.();
	});

	it('treats abort during hygiene sleep trigger as expected restart transition', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const reload = vi.fn();
				const getStatus = vi
					.fn()
					.mockResolvedValueOnce({ uptime_ms: 120000 })
					.mockRejectedValueOnce(new Error('offline'))
					.mockResolvedValueOnce({ uptime_ms: 5000 });

				const sequence = useRestartSequence(
					{
						onSave: vi.fn().mockResolvedValue(undefined),
						triggerRestart: vi
							.fn()
							.mockRejectedValue(new DOMException('Sleep request aborted', 'AbortError')),
						useSleepInsteadOfRestart: true
					},
					{
						reload,
						createPowerApi: () => ({
							getStatus,
							restart: vi.fn().mockResolvedValue(undefined),
							requestHygieneSleepWithTimeout: vi.fn().mockResolvedValue(undefined)
						})
					}
				);

				void sequence.runRestartSequence().then(async () => {
					await flushMicrotasks();
					expect(sequence.stage).toBe('success');
					expect(sequence.errorMessage).toBe('');
					resolve();
				});

				void vi.runAllTimersAsync();
			});
		});

		cleanup?.();
	});

	it('treats network drop during hygiene sleep trigger as expected restart transition', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const reload = vi.fn();
				const getStatus = vi
					.fn()
					.mockResolvedValueOnce({ uptime_ms: 120000 })
					.mockRejectedValueOnce(new Error('offline'))
					.mockResolvedValueOnce({ uptime_ms: 5000 });

				const sequence = useRestartSequence(
					{
						onSave: vi.fn().mockResolvedValue(undefined),
						triggerRestart: vi.fn().mockRejectedValue(new TypeError('Failed to fetch')),
						useSleepInsteadOfRestart: true
					},
					{
						reload,
						createPowerApi: () => ({
							getStatus,
							restart: vi.fn().mockResolvedValue(undefined),
							requestHygieneSleepWithTimeout: vi.fn().mockResolvedValue(undefined)
						})
					}
				);

				void sequence.runRestartSequence().then(async () => {
					await flushMicrotasks();
					expect(sequence.stage).toBe('success');
					expect(sequence.errorMessage).toBe('');
					resolve();
				});

				void vi.runAllTimersAsync();
			});
		});

		cleanup?.();
	});

	it('detects fast hygiene reboot from lower uptime even without observed offline state', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const reload = vi.fn();
				const getStatus = vi
					.fn()
					.mockResolvedValueOnce({ uptime_ms: 240000 })
					.mockResolvedValueOnce({ uptime_ms: 1200 });

				const sequence = useRestartSequence(
					{
						onSave: vi.fn().mockResolvedValue(undefined),
						triggerRestart: vi.fn().mockResolvedValue(undefined),
						useSleepInsteadOfRestart: true
					},
					{
						reload,
						createPowerApi: () => ({
							getStatus,
							restart: vi.fn().mockResolvedValue(undefined),
							requestHygieneSleepWithTimeout: vi.fn().mockResolvedValue(undefined)
						})
					}
				);

				void sequence.runRestartSequence().then(async () => {
					await flushMicrotasks();
					expect(sequence.stage).toBe('success');
					expect(sequence.seenOffline).toBe(false);
					expect(sequence.errorMessage).toBe('');
					resolve();
				});

				void vi.runAllTimersAsync();
			});
		});

		cleanup?.();
	});
});
