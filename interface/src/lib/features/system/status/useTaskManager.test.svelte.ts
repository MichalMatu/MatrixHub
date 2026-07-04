import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useTaskManager } from './useTaskManager.svelte';

const { mockApi, mockCreateService, lastSignals } = vi.hoisted(() => {
	const signals: AbortSignal[] = [];
	const detailsResponse = {
		watchdog: {
			initialized: true,
			timeoutSec: 5
		},
		taskCount: 1,
		detailsIncluded: true,
		memory: {
			freeHeap: 4096,
			minFreeHeap: 2048
		},
		tasks: [
			{
				name: 'main',
				priority: 5,
				stackHighWaterMark: 2048,
				state: 'running',
				coreId: 1
			}
		]
	};

	return {
		mockApi: {
			getTasks: vi.fn((options?: { signal?: AbortSignal; details?: boolean }) => {
				if (options?.signal) {
					signals.push(options.signal);
				}
				return Promise.resolve(detailsResponse);
			})
		},
		mockCreateService: vi.fn(),
		lastSignals: signals,
		detailsResponse
	};
});

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal<typeof import('svelte')>();
	return {
		...actual,
		onDestroy: vi.fn()
	};
});

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn((error: unknown) =>
		error instanceof DOMException && error.name === 'AbortError' ? 'abort' : null
	),
	toUserRequestErrorMessage: vi.fn((error: unknown) =>
		error instanceof Error ? error.message : 'unknown error'
	)
}));

describe('useTaskManager', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		lastSignals.length = 0;
		mockCreateService.mockReturnValue(mockApi);
	});

	it('fetches tasks when modal opens', async () => {
		let open = $state(false);
		let taskManager: ReturnType<typeof useTaskManager> | null = null;

		const cleanup = $effect.root(() => {
			taskManager = useTaskManager({ isOpen: () => open });
		});

		open = true;

		await vi.waitFor(() => {
			expect(mockApi.getTasks).toHaveBeenCalledOnce();
			expect(taskManager?.tasksData?.taskCount).toBe(1);
			expect(taskManager?.tasksData?.detailsIncluded).toBe(true);
			expect(taskManager?.tasksData?.tasks?.[0]?.name).toBe('main');
			expect(taskManager?.loading).toBe(false);
		});
		expect(mockApi.getTasks).toHaveBeenCalledWith(expect.objectContaining({ details: true }));

		cleanup();
	});

	it('aborts inflight request when modal closes', async () => {
		let pendingSignal: AbortSignal | undefined;

		mockApi.getTasks.mockImplementation(
			(options?: { signal?: AbortSignal }) =>
				new Promise((resolve, reject) => {
					pendingSignal = options?.signal;
					options?.signal?.addEventListener('abort', () => {
						reject(new DOMException('Aborted', 'AbortError'));
					});
				})
		);

		let open = $state(false);
		const cleanup = $effect.root(() => {
			useTaskManager({ isOpen: () => open });
		});

		open = true;
		await vi.waitFor(() => {
			expect(mockApi.getTasks).toHaveBeenCalledOnce();
			expect(pendingSignal).toBeTruthy();
		});

		open = false;

		await vi.waitFor(() => {
			expect(pendingSignal?.aborted).toBe(true);
		});

		cleanup();
	});
});
