import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useLiveTailManagement } from './useLiveTailManagement.svelte';

const { mockApi, mockCreateService, mockPollerStart, mockPollerStop } = vi.hoisted(() => {
	return {
		mockApi: {
			getLogTail: vi.fn(),
			getConfig: vi.fn(),
			saveConfig: vi.fn(),
			clearLogTail: vi.fn()
		},
		mockCreateService: vi.fn(),
		mockPollerStart: vi.fn(),
		mockPollerStop: vi.fn()
	};
});

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$lib/utils/api/usePolling.svelte', () => ({
	usePolling: () => ({
		start: mockPollerStart,
		stop: mockPollerStop
	})
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown error';
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', async (importOriginal) => {
	const actual = await importOriginal<typeof import('$lib/paraglide/messages.js')>();
	return {
		...actual,
		livetail_error_tail_timeout: () => 'tail timeout' as never,
		livetail_error_tail_fallback: () => 'tail failed' as never,
		livetail_error_save: () => 'save failed' as never,
		livetail_error_clear: () => 'clear failed' as never
	};
});

describe('useLiveTailManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.useRealTimers();
		mockCreateService.mockReturnValue(mockApi);
		mockApi.getLogTail.mockResolvedValue({
			capacity: 500,
			lines: [{ t: 1000, l: 'I', g: 'SYS', m: 'boot ok' }]
		});
		mockApi.getConfig.mockResolvedValue({ logging: { level: 'warn' } });
		mockApi.saveConfig.mockResolvedValue(undefined);
		mockApi.clearLogTail.mockResolvedValue(undefined);
	});

	it('loads config over HTTP and saves updated logging level', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const liveTail = useLiveTailManagement();
				liveTail.init();

				void vi
					.waitFor(() => {
						expect(mockApi.getConfig).toHaveBeenCalledOnce();
						expect(liveTail.loggingConfig.level).toBe('warn');
						expect(liveTail.isLoggingConfigLoaded).toBe(true);
					})
					.then(() => {
						liveTail.loggingConfig.level = 'error';
						expect(liveTail.isDirty).toBe(true);
						return liveTail.saveLoggingSettings();
					})
					.then(() => {
						expect(mockApi.saveConfig).toHaveBeenCalledWith({ logging: { level: 'error' } });
						expect(liveTail.isDirty).toBe(false);
						liveTail.destroy();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('keeps logging config unloaded when the first config request fails', async () => {
		mockApi.getConfig.mockRejectedValueOnce(new Error('config failed'));
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const liveTail = useLiveTailManagement();
				liveTail.init();

				void vi
					.waitFor(() => {
						expect(mockApi.getConfig).toHaveBeenCalledOnce();
						expect(liveTail.isLoggingConfigLoaded).toBe(false);
						expect(liveTail.configError).toBe('config failed');
					})
					.then(() => {
						liveTail.destroy();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('resets edited logging config to the loaded level', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const liveTail = useLiveTailManagement();
				liveTail.init();

				void vi
					.waitFor(() => {
						expect(liveTail.loggingConfig.level).toBe('warn');
						expect(liveTail.isLoggingConfigLoaded).toBe(true);
					})
					.then(() => {
						liveTail.loggingConfig.level = 'debug';
						expect(liveTail.isDirty).toBe(true);

						liveTail.resetLoggingSettings();

						expect(liveTail.loggingConfig.level).toBe('warn');
						expect(liveTail.isDirty).toBe(false);
						expect(mockApi.saveConfig).not.toHaveBeenCalled();
						liveTail.destroy();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('starts delayed tail refresh and exposes fetched tail data', async () => {
		vi.useFakeTimers();
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const liveTail = useLiveTailManagement();
				liveTail.init();
				liveTail.start();

				void vi
					.advanceTimersByTimeAsync(800)
					.then(() => {
						return vi.waitFor(() => {
							expect(mockPollerStart).toHaveBeenCalledOnce();
							expect(liveTail.capacity).toBe(500);
							expect(liveTail.tail).toHaveLength(1);
							expect(liveTail.tail[0]).toMatchObject({
								level: 'I',
								tag: 'SYS',
								message: 'boot ok'
							});
						});
					})
					.then(() => {
						liveTail.togglePause();
						expect(mockPollerStop).toHaveBeenCalled();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('clears backend tail buffer and resets local entries', async () => {
		vi.useFakeTimers();
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const liveTail = useLiveTailManagement();
				liveTail.init();
				liveTail.start();

				void vi
					.advanceTimersByTimeAsync(800)
					.then(() =>
						vi.waitFor(() => {
							expect(liveTail.tail).toHaveLength(1);
						})
					)
					.then(async () => {
						await liveTail.clear();
						expect(mockApi.clearLogTail).toHaveBeenCalledOnce();
						expect(liveTail.tail).toHaveLength(0);
						resolve();
					});
			});
		});

		cleanup?.();
	});
});
