import {
	FILE_MANAGER_SDCARD_PATH,
	canonicalizeFileManagerPath,
	createFileManagerSystemState,
	validateFileManagerPathAccess
} from './utils';
import type {
	CreateFileManagerActionsOptions,
	FileEntry,
	RawFileEntry,
	FileManagerSystemState
} from './types';
import type { SystemInformation } from '$lib/types/system/system';
import type { SystemStatusSnapshot } from '$lib/types/system/systemStatusSnapshot';
import type { ApiClient } from '$lib/utils';
import { basename } from '$lib/utils/path/helpers';
import { toUserRequestErrorMessage } from '$lib/utils/api/requestErrors';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { showError, showSuccess } from '$lib/utils/ui/notifications';
import { Logger } from '$lib/services/core/Logger';
import { systemStatus } from '$lib/stores/systemStatus.svelte';

type DeleteResponse = { ok?: boolean; error?: string };

export async function fetchFileManagerSystemState(
	apiClient: ApiClient
): Promise<FileManagerSystemState> {
	try {
		const cachedSnapshot = systemStatus.getSnapshot<SystemStatusSnapshot>('system_status');
		if (cachedSnapshot?.system_info) {
			return createFileManagerSystemState(cachedSnapshot.system_info as SystemInformation);
		}

		const data = await apiClient.get('/api/system/info');

		if (!data) {
			Logger.error('Received invalid storage status payload.', data);
			return createFileManagerSystemState();
		}

		return createFileManagerSystemState(data as SystemInformation);
	} catch (error) {
		Logger.error('Failed to load storage status', error);
		return createFileManagerSystemState();
	}
}

export function createFileManagerActions(
	{
		defaultPath,
		directoryCache,
		checkIfDirectory,
		callbacks,
		apiClient
	}: CreateFileManagerActionsOptions,
	{
		fetchSystemState = fetchFileManagerSystemState
	}: { fetchSystemState?: typeof fetchFileManagerSystemState } = {}
) {
	const getLocaleOptions = () => ({ locale: i18n.languageTag });
	const getToastMessage = (message: string) => m.toast_message({ message }, getLocaleOptions());
	const getFileManagerErrorMessage = (error: unknown, fallbackMessage: string) =>
		toUserRequestErrorMessage(error, { fallbackMessage });
	const getPathValidationMessage = (
		errorCode: 'fs/invalid_path' | 'fs/path_forbidden',
		fallbackMessage: string
	) => getFileManagerErrorMessage(new Error(errorCode), fallbackMessage);

	async function getResponseErrorMessage(response: Response, fallbackMessage: string) {
		try {
			const errData = (await response.json()) as { error?: string };
			if (typeof errData?.error === 'string') {
				return getFileManagerErrorMessage(new Error(errData.error), fallbackMessage);
			}
		} catch {
			// Ignore non-JSON error bodies
		}
		return fallbackMessage;
	}

	function sanitizeEntry(file: RawFileEntry): FileEntry | null {
		if (!file.name.trim()) {
			return null;
		}

		const pathResult = canonicalizeFileManagerPath(file.name);
		if (!pathResult.ok) {
			return null;
		}

		return {
			...file,
			name: pathResult.path,
			type: file.directory ? 'directory' : file.size === 0 ? 'unknown' : 'file'
		};
	}

	async function refreshSystemState() {
		const next = await fetchSystemState(apiClient);
		callbacks.setSystemState(next);
	}

	async function loadDirectory(path: string) {
		callbacks.setIsLoading(true);
		callbacks.setErrorMessage(null);

		const directoryResult = validateFileManagerPathAccess(path, 'list', defaultPath);
		if (!directoryResult.ok) {
			callbacks.setErrorMessage(
				getPathValidationMessage(directoryResult.error, 'Failed to load directory contents.')
			);
			callbacks.setIsLoading(false);
			return;
		}

		const safePath = directoryResult.path;

		try {
			const data = await apiClient.get(`/rest/fs/list?dir=${encodeURIComponent(safePath)}`);

			const rawFiles: RawFileEntry[] = (data as { files?: RawFileEntry[] })?.files ?? [];
			const files: FileEntry[] = rawFiles.flatMap((file) => {
				const entry = sanitizeEntry(file);
				if (
					!entry ||
					entry.name === FILE_MANAGER_SDCARD_PATH ||
					entry.name.startsWith(`${FILE_MANAGER_SDCARD_PATH}/`)
				) {
					return [];
				}
				return [entry];
			});

			files.forEach((entry: FileEntry) => {
				directoryCache.set(entry.name, entry.type === 'directory');
			});

			callbacks.setCurrentPath(safePath);
			callbacks.setEntries(files);

			const pending = files.filter((entry: FileEntry) => entry.type === 'unknown');

			if (pending.length) {
				await Promise.allSettled(
					pending.map(async (entry: FileEntry) => {
						const isDir = await checkIfDirectory(entry.name);
						entry.type = isDir ? 'directory' : 'file';
					})
				);
				callbacks.setEntries([...files]);
			}
		} catch (error) {
			Logger.error('Failed to load directory', error);
			callbacks.setErrorMessage(
				getFileManagerErrorMessage(error, 'Failed to load directory contents.')
			);
		} finally {
			callbacks.setIsLoading(false);
		}
	}

	async function downloadEntry(entry: FileEntry) {
		const pathResult = canonicalizeFileManagerPath(entry.name);
		if (!pathResult.ok) {
			showError(
				getToastMessage(
					getPathValidationMessage(
						pathResult.error,
						m.file_manager_download_failed({}, getLocaleOptions())
					)
				)
			);
			return;
		}

		const safePath = pathResult.path;

		try {
			const response = await apiClient.fetch(
				`/rest/fs/download?path=${encodeURIComponent(safePath)}`,
				{
					method: 'GET',
					headers: {
						Accept: 'application/octet-stream'
					}
				}
			);

			if (!response.ok) {
				showError(
					getToastMessage(
						await getResponseErrorMessage(
							response,
							m.file_manager_download_failed({}, getLocaleOptions())
						)
					)
				);
				return;
			}

			const blob = await response.blob();
			const url = URL.createObjectURL(blob);
			const link = document.createElement('a');
			link.href = url;
			link.download = basename(safePath);
			document.body.appendChild(link);
			link.click();
			document.body.removeChild(link);
			URL.revokeObjectURL(url);
			showSuccess(m.file_manager_download_success({}, getLocaleOptions()));
		} catch (error) {
			Logger.error('Failed to download file', error);
			showError(
				getToastMessage(
					getFileManagerErrorMessage(error, m.file_manager_download_failed({}, getLocaleOptions()))
				)
			);
		}
	}

	async function refreshCurrentDirectory() {
		const path = callbacks.getCurrentPath() || defaultPath;
		await Promise.all([loadDirectory(path), refreshSystemState()]);
	}

	async function deleteEntry(entry: FileEntry) {
		const pathResult = validateFileManagerPathAccess(entry.name, 'remove');
		if (!pathResult.ok) {
			showError(
				getToastMessage(
					getPathValidationMessage(
						pathResult.error,
						m.file_manager_delete_failed({}, getLocaleOptions())
					)
				)
			);
			return;
		}

		try {
			await apiClient.delete<DeleteResponse>(
				`/rest/fs/remove?path=${encodeURIComponent(pathResult.path)}`
			);

			await refreshCurrentDirectory();
			showSuccess(m.file_manager_delete_success({}, getLocaleOptions()));
		} catch (error) {
			Logger.error('Failed to remove entry', error);
			showError(
				getToastMessage(
					getFileManagerErrorMessage(error, m.file_manager_delete_failed({}, getLocaleOptions()))
				)
			);
		}
	}

	async function handleUpload() {
		const files = callbacks.getUploadFiles();
		if (!files || files.length === 0) {
			return;
		}

		const file = files[0];
		const targetPath = callbacks.getCurrentPath() || defaultPath;
		const targetPathResult = validateFileManagerPathAccess(targetPath, 'upload', defaultPath);
		if (!targetPathResult.ok) {
			showError(
				getToastMessage(
					getPathValidationMessage(
						targetPathResult.error,
						m.file_manager_upload_failed({}, getLocaleOptions())
					)
				)
			);
			return;
		}

		callbacks.setUploading(true);

		try {
			const endpoint = `/rest/fs/upload?path=${encodeURIComponent(targetPathResult.path)}`;

			const formData = new FormData();
			formData.append('file', file);

			// Upload needs a longer timeout than the default 15s API timeout.
			// Large files over WiFi to ESP32 can take 30-60+ seconds.
			const uploadSignal = AbortSignal.timeout(120_000); // 2 minutes

			const response = await apiClient.fetch(endpoint, {
				method: 'POST',
				body: formData,
				signal: uploadSignal
			});

			if (response.ok) {
				showSuccess(m.file_manager_upload_success({}, getLocaleOptions()));
				callbacks.setUploadFiles(null);
				await refreshCurrentDirectory();
			} else {
				showError(
					getToastMessage(
						await getResponseErrorMessage(
							response,
							m.file_manager_upload_failed({}, getLocaleOptions())
						)
					)
				);
			}
		} catch (error: unknown) {
			Logger.error('Failed to upload file', error);
			const isTimeout =
				(error instanceof Error && error.name === 'TimeoutError') ||
				(error instanceof Error && error.message.includes('timeout'));
			const msg = isTimeout
				? m.file_manager_upload_timeout({}, getLocaleOptions())
				: getFileManagerErrorMessage(error, m.file_manager_upload_failed({}, getLocaleOptions()));
			showError(getToastMessage(msg as string));
		} finally {
			callbacks.setUploading(false);
		}
	}

	return {
		loadDirectory,
		downloadEntry,
		deleteEntry,
		handleUpload,
		refreshSystemState
	};
}
