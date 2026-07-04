// @vitest-environment jsdom
import { cleanup, render } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import UsePollingHarness from './UsePollingHarness.svelte';

interface PollingHarnessController {
	start: () => void;
	stop: () => void;
	poll: () => Promise<void>;
	isPolling: () => boolean;
	isFetching: () => boolean;
}

describe('usePolling', () => {
	afterEach(() => {
		cleanup();
		vi.useRealTimers();
	});

	it('does not create duplicate intervals when start is called repeatedly before jitter elapses', async () => {
		vi.useFakeTimers();
		const fetchFn = vi.fn().mockResolvedValue(undefined);
		let controller: PollingHarnessController | undefined;

		render(UsePollingHarness, {
			props: {
				fetchFn,
				options: {
					autoStart: false,
					initialPoll: false,
					intervalMs: 1000,
					jitter: true
				},
				onReady: (nextController: PollingHarnessController) => {
					controller = nextController;
				}
			}
		});

		expect(controller).toBeTruthy();

		controller!.start();
		controller!.start();

		expect(controller!.isPolling()).toBe(true);

		await vi.advanceTimersByTimeAsync(300);
		await vi.advanceTimersByTimeAsync(1000);

		expect(fetchFn).toHaveBeenCalledTimes(1);
	});

	it('cancels a scheduled polling start when stop is called before jitter elapses', async () => {
		vi.useFakeTimers();
		const fetchFn = vi.fn().mockResolvedValue(undefined);
		let controller: PollingHarnessController | undefined;

		render(UsePollingHarness, {
			props: {
				fetchFn,
				options: {
					autoStart: false,
					initialPoll: false,
					intervalMs: 1000,
					jitter: true
				},
				onReady: (nextController: PollingHarnessController) => {
					controller = nextController;
				}
			}
		});

		expect(controller).toBeTruthy();

		controller!.start();
		expect(controller!.isPolling()).toBe(true);

		controller!.stop();
		expect(controller!.isPolling()).toBe(false);

		await vi.advanceTimersByTimeAsync(300);
		await vi.advanceTimersByTimeAsync(1000);

		expect(fetchFn).not.toHaveBeenCalled();
	});
});
