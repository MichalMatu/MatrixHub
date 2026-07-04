import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { LogListResponse } from '$lib/services/api/monitoring/LogsApiService';

const { loggerError, mockNotifications, mockModalService } = vi.hoisted(() => ({
	loggerError: vi.fn(),
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn()
	},
	mockModalService: {
		open: vi.fn()
	}
}));

vi.mock('svelte-modals', () => ({
	modals: {
		open: vi.fn()
	}
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: {}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn(
		(
			options: { modalService?: typeof mockModalService; component?: unknown } & Record<
				string,
				unknown
			>
		) => {
			const { modalService = mockModalService, component = {}, ...props } = options;
			return modalService.open(component, props);
		}
	)
}));

vi.mock('$lib/utils', () => ({
	escapeHtml: vi.fn((value: string) => value),
	getRequestAbortKind: vi.fn((error: unknown) =>
		typeof error === 'object' && error !== null && 'kind' in error
			? (error as { kind: string }).kind
			: null
	),
	toUserRequestErrorMessage: vi.fn(
		(error: unknown, options?: { fallbackMessage?: string; timeoutMessage?: string }) => {
			if (error instanceof Error && error.message) return error.message;
			return options?.fallbackMessage ?? 'unknown';
		}
	)
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: loggerError
	}
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	logs_list_timeout: () => 'list timeout',
	logs_list_failed: () => 'list failed',
	logs_download_started: () => 'download started',
	logs_download_success: () => 'download success',
	logs_download_timeout: () => 'download timeout',
	logs_download_failed: () => 'download failed',
	logs_delete_title: () => 'Delete log',
	logs_delete_msg: ({ name }: { name: string }) => `delete ${name}`,
	logs_delete_success: () => 'delete success',
	logs_delete_timeout: () => 'delete timeout',
	logs_delete_failed: () => 'delete failed',
	toast_message: ({ message }: { message: string }) => `toast: ${message}`
}));

function createLogsResponse(files: Array<{ name: string; size: number }> = []): LogListResponse {
	return {
		months: [
			{
				name: '2026-03',
				path: '/data/2026-03',
				files
			}
		]
	};
}

function flushPromises() {
	return new Promise((resolve) => setTimeout(resolve, 0));
}

function createAccess(canRead: boolean, canManage: boolean, bearerToken = 'token') {
	return {
		canRead,
		canManage,
		apiOptions: { bearerToken }
	};
}

describe('useLogsManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('skips admin-only API calls when user cannot manage logs', async () => {
		const { useLogsManagement } = await import('./useLogsManagement.svelte');
		const api = {
			getLogsList: vi.fn(),
			downloadLog: vi.fn(),
			deleteLog: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const logs = useLogsManagement({
					access: createAccess(false, false),
					createApi: () => api as never
				});

				void vi
					.waitFor(() => {
						expect(api.getLogsList).not.toHaveBeenCalled();
						expect(logs.loading).toBe(false);
						expect(logs.logs).toEqual({ months: [] });
					})
					.then(() => {
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('loads logs list for admin users', async () => {
		const { useLogsManagement } = await import('./useLogsManagement.svelte');
		const api = {
			getLogsList: vi.fn().mockResolvedValue(createLogsResponse([{ name: 'day.bin', size: 1024 }])),
			downloadLog: vi.fn(),
			deleteLog: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const logs = useLogsManagement({
					access: createAccess(true, true),
					createApi: () => api as never
				});

				void vi
					.waitFor(() => {
						expect(api.getLogsList).toHaveBeenCalledOnce();
						expect(logs.logs.months[0]?.files).toHaveLength(1);
						expect(logs.error).toBeNull();
					})
					.then(() => {
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('downloads a protected log through the API client', async () => {
		const { useLogsManagement } = await import('./useLogsManagement.svelte');
		const blob = new Blob(['demo']);
		const api = {
			getLogsList: vi.fn().mockResolvedValue(createLogsResponse([{ name: 'day.bin', size: 1024 }])),
			downloadLog: vi.fn().mockResolvedValue(blob),
			deleteLog: vi.fn()
		};
		const downloadBlob = vi.fn();

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const logs = useLogsManagement({
					access: createAccess(true, false),
					createApi: () => api as never,
					downloadBlob
				});

				void vi
					.waitFor(() => {
						expect(api.getLogsList).toHaveBeenCalledOnce();
					})
					.then(async () => {
						await logs.downloadLog('/data/2026-03', 'day.bin');

						expect(api.downloadLog).toHaveBeenCalledWith('/data/2026-03/day.bin');
						expect(downloadBlob).toHaveBeenCalledWith(blob, 'day.bin');
						expect(mockNotifications.success).toHaveBeenCalledWith('download success', 3000);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('keeps delete blocked in read-only mode', async () => {
		const { useLogsManagement } = await import('./useLogsManagement.svelte');
		const api = {
			getLogsList: vi.fn().mockResolvedValue(createLogsResponse([{ name: 'day.bin', size: 1024 }])),
			downloadLog: vi.fn(),
			deleteLog: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const logs = useLogsManagement({
					access: createAccess(true, false),
					createApi: () => api as never,
					modalService: mockModalService
				});

				void vi
					.waitFor(() => {
						expect(api.getLogsList).toHaveBeenCalledOnce();
					})
					.then(() => {
						logs.confirmDelete('/data/2026-03', 'day.bin');

						expect(mockModalService.open).not.toHaveBeenCalled();
						expect(api.deleteLog).not.toHaveBeenCalled();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('opens delete confirmation and refreshes logs after delete', async () => {
		const { useLogsManagement } = await import('./useLogsManagement.svelte');
		const initialLogs = createLogsResponse([
			{ name: 'day.bin', size: 1024 },
			{ name: 'week.bin', size: 2048 }
		]);
		const refreshedLogs = createLogsResponse([{ name: 'week.bin', size: 2048 }]);
		const api = {
			getLogsList: vi.fn().mockResolvedValueOnce(initialLogs).mockResolvedValueOnce(refreshedLogs),
			downloadLog: vi.fn(),
			deleteLog: vi.fn().mockResolvedValue(undefined)
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const logs = useLogsManagement({
					access: createAccess(true, true),
					createApi: () => api as never,
					modalService: mockModalService
				});

				void vi
					.waitFor(() => {
						expect(api.getLogsList).toHaveBeenCalledOnce();
					})
					.then(async () => {
						logs.confirmDelete('/data/2026-03', 'day.bin');

						const payload = mockModalService.open.mock.calls[0]?.[1] as { onConfirm?: () => void };
						payload.onConfirm?.();
						await flushPromises();

						expect(api.deleteLog).toHaveBeenCalledWith('/data/2026-03/day.bin');
						expect(api.getLogsList).toHaveBeenCalledTimes(2);
						expect(logs.logs.months[0]?.files).toEqual([{ name: 'week.bin', size: 2048 }]);
						expect(mockNotifications.success).toHaveBeenCalledWith('delete success', 3000);
						resolve();
					});
			});
		});

		cleanup?.();
	});
});
