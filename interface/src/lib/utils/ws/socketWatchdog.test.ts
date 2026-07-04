import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { SocketWatchdog } from './socketWatchdog';

describe('SocketWatchdog', () => {
	beforeEach(() => {
		vi.useFakeTimers();
	});

	afterEach(() => {
		vi.useRealTimers();
	});

	it('fires once after the timeout', async () => {
		const onTimeout = vi.fn();
		const watchdog = new SocketWatchdog({
			timeoutMs: 1000,
			onTimeout
		});

		watchdog.arm();
		await vi.advanceTimersByTimeAsync(1000);

		expect(onTimeout).toHaveBeenCalledOnce();
	});

	it('restarts the timer when armed again', async () => {
		const onTimeout = vi.fn();
		const watchdog = new SocketWatchdog({
			timeoutMs: 1000,
			onTimeout
		});

		watchdog.arm();
		await vi.advanceTimersByTimeAsync(500);
		watchdog.arm();
		await vi.advanceTimersByTimeAsync(500);

		expect(onTimeout).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(500);
		expect(onTimeout).toHaveBeenCalledOnce();
	});

	it('stops after clear', async () => {
		const onTimeout = vi.fn();
		const watchdog = new SocketWatchdog({
			timeoutMs: 1000,
			onTimeout
		});

		watchdog.arm();
		watchdog.clear();
		await vi.advanceTimersByTimeAsync(1000);

		expect(onTimeout).not.toHaveBeenCalled();
	});
});
