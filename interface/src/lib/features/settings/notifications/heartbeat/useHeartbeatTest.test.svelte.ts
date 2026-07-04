import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockNotifications, loggerError } = vi.hoisted(() => ({
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn()
	},
	loggerError: vi.fn()
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	heartbeat_test_success: () => 'ping sent',
	toast_test_failed: () => 'test failed',
	toast_test_in_progress: () => 'test in progress',
	toast_request_timeout: () => 'request timeout',
	restart_error_cancelled: () => 'request cancelled',
	request_error_network: () => 'network error',
	request_error_failed: () => 'request failed',
	toast_message: ({ message }: { message: string }) => `toast: ${message}`
}));

describe('useHeartbeatTest', () => {
	beforeEach(async () => {
		vi.clearAllMocks();
		const { resetGlobalTestExecutionGateForTests } = await import('../../testExecutionGate.svelte');
		resetGlobalTestExecutionGateForTests();
	});

	it('shows a success toast after sending a ping', async () => {
		const { useHeartbeatTest } = await import('./useHeartbeatTest.svelte');
		const api = {
			testPing: vi.fn().mockResolvedValue({ success: true, message: 'ping_triggered' })
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const heartbeatTest = useHeartbeatTest({
					api,
					notifications: mockNotifications,
					logger: { error: loggerError }
				});

				void heartbeatTest.sendTest().then(() => {
					expect(api.testPing).toHaveBeenCalledWith();
					expect(mockNotifications.success).toHaveBeenCalledWith('ping sent', 4000);
					expect(heartbeatTest.lastError).toBeNull();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('guards against concurrent sends', async () => {
		const { useHeartbeatTest } = await import('./useHeartbeatTest.svelte');
		let resolveRequest: (() => void) | undefined;
		const api = {
			testPing: vi.fn().mockImplementation(
				() =>
					new Promise<{ success: boolean; message: string }>((resolve) => {
						resolveRequest = () => resolve({ success: true, message: 'ok' });
					})
			)
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const heartbeatTest = useHeartbeatTest({
					api,
					notifications: mockNotifications,
					logger: { error: loggerError }
				});

				void heartbeatTest.sendTest();
				void heartbeatTest.sendTest();

				setTimeout(() => {
					expect(api.testPing).toHaveBeenCalledTimes(1);
					resolveRequest?.();
					setTimeout(resolve, 0);
				}, 0);
			});
		});

		cleanup?.();
	});

	it('maps request failures to user-facing error toasts', async () => {
		const { useHeartbeatTest } = await import('./useHeartbeatTest.svelte');
		const api = {
			testPing: vi.fn().mockRejectedValue(new Error('heartbeat/no_enabled_slots'))
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const heartbeatTest = useHeartbeatTest({
					api,
					notifications: mockNotifications,
					logger: { error: loggerError }
				});

				void heartbeatTest.sendTest().then(() => {
					expect(loggerError).toHaveBeenCalled();
					expect(heartbeatTest.lastError).toBe('heartbeat/no_enabled_slots');
					expect(mockNotifications.error).toHaveBeenCalledWith(
						'toast: heartbeat/no_enabled_slots',
						4000
					);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
