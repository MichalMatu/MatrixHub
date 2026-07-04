import { ConfirmDialog } from '$lib/components';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { LogsApiService, type LogListResponse } from '$lib/services/api/monitoring/LogsApiService';
import { Logger } from '$lib/services/core/Logger';
import { escapeHtml, getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { confirm } from '$lib/utils/ui/dialogs';
import type { ModalOpenService } from '$lib/utils/ui/modal';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

type LogsManagementDeps = {
	access?: Pick<SessionAccess, 'canRead' | 'canManage' | 'apiOptions'>;
	createApi?: () => LogsApiService;
	notifications?: Pick<typeof notifications, 'success' | 'error'>;
	modalService?: ModalOpenService;
	confirmDialogComponent?: unknown;
	logger?: Pick<typeof Logger, 'error'>;
	downloadBlob?: (blob: Blob, filename: string) => void;
};

function defaultDownloadBlob(blob: Blob, filename: string) {
	const link = document.createElement('a');
	link.href = URL.createObjectURL(blob);
	link.download = filename;
	link.click();
	URL.revokeObjectURL(link.href);
}

export function useLogsManagement(deps: LogsManagementDeps = {}) {
	const access = deps.access ?? useSessionAccess();
	const apiClient = deps.createApi ? null : useApiClient();
	const toast = deps.notifications ?? notifications;
	const modalService = deps.modalService;
	const confirmDialogComponent = deps.confirmDialogComponent ?? ConfirmDialog;
	const logger = deps.logger ?? Logger;
	const downloadBlob = deps.downloadBlob ?? defaultDownloadBlob;

	function createApi() {
		return deps.createApi?.() ?? apiClient!.createService(LogsApiService);
	}

	let logs = $state<LogListResponse>({ months: [] });
	let loading = $state(true);
	let error = $state<string | null>(null);

	async function syncAccessState(): Promise<void> {
		if (!access.canRead) {
			loading = false;
			error = null;
			logs = { months: [] };
			return;
		}

		await refreshLogs();
	}

	$effect(() => {
		void syncAccessState();
	});

	async function refreshLogs(): Promise<void> {
		if (!access.canRead) {
			loading = false;
			return;
		}

		loading = true;
		try {
			const data = await createApi().getLogsList();
			logs = data;
			error = null;
		} catch (cause) {
			logger.error('Logs fetch error:', cause);
			const kind = getRequestAbortKind(cause);
			if (kind === 'abort') return;
			error = toUserRequestErrorMessage(cause, {
				timeoutMessage: m.logs_list_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.logs_list_failed({ locale: i18n.languageTag })
			});
		} finally {
			loading = false;
		}
	}

	async function downloadLog(monthPath: string, filename: string): Promise<void> {
		if (!access.canRead) return;

		const fullPath = `${monthPath}/${filename}`;

		try {
			const blob = await createApi().downloadLog(fullPath);
			downloadBlob(blob, filename);
			toast.success(m.logs_download_success({ locale: i18n.languageTag }), 3000);
		} catch (cause) {
			logger.error('[Download] Error:', cause);
			const kind = getRequestAbortKind(cause);
			if (kind === 'abort') return;
			const message = toUserRequestErrorMessage(cause, {
				timeoutMessage: m.logs_download_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.logs_download_failed({ locale: i18n.languageTag })
			});
			toast.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		}
	}

	function confirmDelete(monthPath: string, filename: string) {
		if (!access.canManage) return;

		confirm({
			title: m.logs_delete_title({ locale: i18n.languageTag }),
			messageHtml: m.logs_delete_msg({ name: escapeHtml(filename) }, { locale: i18n.languageTag }),
			onConfirm: () => {
				void deleteLog(monthPath, filename);
			},
			component: confirmDialogComponent,
			modalService
		});
	}

	async function deleteLog(monthPath: string, filename: string): Promise<void> {
		if (!access.canManage) return;

		const fullPath = `${monthPath}/${filename}`;

		try {
			await createApi().deleteLog(fullPath);
			await refreshLogs();
			toast.success(m.logs_delete_success({ locale: i18n.languageTag }), 3000);
		} catch (cause) {
			logger.error('[Delete] Error:', cause);
			const kind = getRequestAbortKind(cause);
			if (kind === 'abort') return;
			const message = toUserRequestErrorMessage(cause, {
				timeoutMessage: m.logs_delete_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.logs_delete_failed({ locale: i18n.languageTag })
			});
			toast.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		}
	}

	return {
		get logs() {
			return logs;
		},
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		get canManage() {
			return access.canManage;
		},
		get canRead() {
			return access.canRead;
		},
		refreshLogs,
		downloadLog,
		confirmDelete,
		deleteLog
	};
}
