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
	udp_test_success: ({ host, port }: { host: string; port: string }) => `sent ${host}:${port}`,
	toast_test_failed: () => 'test failed',
	toast_test_in_progress: () => 'test in progress',
	toast_request_timeout: () => 'request timeout',
	restart_error_cancelled: () => 'request cancelled',
	request_error_network: () => 'network error',
	request_error_failed: () => 'request failed',
	toast_message: ({ message }: { message: string }) => `toast: ${message}`
}));

describe('useUdpTest', () => {
	beforeEach(async () => {
		vi.clearAllMocks();
		const { resetGlobalTestExecutionGateForTests } = await import('../../testExecutionGate.svelte');
		resetGlobalTestExecutionGateForTests();
	});

	it('shows a success toast with current host and port', async () => {
		const { useUdpTest } = await import('./useUdpTest.svelte');
		const api = {
			testSend: vi.fn().mockResolvedValue({ success: true, message: 'UDP packet sent' })
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const testState = useUdpTest(() => ({ host: 'demo.local', port: 9000 }), {
					api,
					notifications: mockNotifications,
					logger: { error: loggerError }
				});

				void testState.sendTest().then(() => {
					expect(api.testSend).toHaveBeenCalledWith();
					expect(mockNotifications.success).toHaveBeenCalledWith('sent demo.local:9000', 4000);
					expect(testState.lastError).toBeNull();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('guards against concurrent test sends', async () => {
		const { useUdpTest } = await import('./useUdpTest.svelte');
		let resolveRequest: (() => void) | undefined;
		const api = {
			testSend: vi.fn().mockImplementation(
				() =>
					new Promise<{ success: boolean; message: string }>((resolve) => {
						resolveRequest = () => resolve({ success: true, message: 'ok' });
					})
			)
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const testState = useUdpTest(() => ({ host: 'demo.local', port: 9000 }), {
					api,
					notifications: mockNotifications,
					logger: { error: loggerError }
				});

				void testState.sendTest();
				void testState.sendTest();

				setTimeout(() => {
					expect(api.testSend).toHaveBeenCalledTimes(1);
					resolveRequest?.();
					setTimeout(resolve, 0);
				}, 0);
			});
		});

		cleanup?.();
	});

	it('maps request failures to user-facing error toasts', async () => {
		const { useUdpTest } = await import('./useUdpTest.svelte');
		const api = {
			testSend: vi.fn().mockRejectedValue(new Error('UDP not configured'))
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const testState = useUdpTest(() => ({ host: 'demo.local', port: 9000 }), {
					api,
					notifications: mockNotifications,
					logger: { error: loggerError }
				});

				void testState.sendTest().then(() => {
					expect(loggerError).toHaveBeenCalled();
					expect(testState.lastError).toBe('UDP not configured');
					expect(mockNotifications.error).toHaveBeenCalledWith('toast: UDP not configured', 4000);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
