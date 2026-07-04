import {
	canonicalizeFileManagerPath,
	createFileManagerSystemState,
	createFileManagerActions,
	isFileManagerReadOnlyPath,
	type FileEntry
} from '$lib/services/fileManager';
import type { ApiClient } from '$lib/utils';
import { parentPath } from '$lib/utils/path/helpers';
import { Logger } from '$lib/services/core/Logger';
import {
	formatEntrySize as baseFormatEntrySize,
	getEntryLabel as baseGetEntryLabel
} from '../selectors/formatters';

interface CreateFileManagerStateOptions {
	initialPath?: string;
	initialEntries?: FileEntry[];
	apiClient: ApiClient;
}

const DEFAULT_PATH = '/';

export function createFileManagerState({
	initialPath = DEFAULT_PATH,
	initialEntries = [],
	apiClient
}: CreateFileManagerStateOptions) {
	const directoryCache = new Map<string, boolean>([[DEFAULT_PATH, true]]);

	const state = $state({
		currentPath: initialPath,
		entries: initialEntries,
		isLoading: false,
		errorMessage: null as string | null,
		uploadFiles: null as FileList | null,
		uploading: false,
		systemState: createFileManagerSystemState()
	});

	function setUploadFiles(files: FileList | null) {
		state.uploadFiles = files;
	}

	async function checkIfDirectory(path: string): Promise<boolean> {
		const pathResult = canonicalizeFileManagerPath(path, DEFAULT_PATH);
		if (!pathResult.ok) {
			return false;
		}

		const safePath = pathResult.path;

		if (directoryCache.has(safePath)) {
			return directoryCache.get(safePath) ?? false;
		}

		if (safePath === DEFAULT_PATH) {
			directoryCache.set(safePath, true);
			return true;
		}

		try {
			const data = await apiClient.get(`/rest/fs/list?dir=${encodeURIComponent(safePath)}`);
			const files = (data as { files?: { name: string }[] })?.files ?? [];
			let isDir = false;
			if (files.length === 0) {
				isDir = true;
			} else if (files.length === 1 && files[0]?.name === safePath) {
				isDir = false;
			} else {
				isDir = true;
			}
			directoryCache.set(safePath, isDir);
			return isDir;
		} catch (error) {
			Logger.error(`[FileManagerState] Failed to determine entry type for ${safePath}`, error);
			directoryCache.set(safePath, false);
			return false;
		}
	}

	const { loadDirectory, downloadEntry, deleteEntry, handleUpload, refreshSystemState } =
		createFileManagerActions({
			defaultPath: DEFAULT_PATH,
			directoryCache,
			apiClient,
			async checkIfDirectory(path: string) {
				return checkIfDirectory(path);
			},
			callbacks: {
				setIsLoading(value) {
					state.isLoading = value;
				},
				setErrorMessage(message) {
					state.errorMessage = message;
				},
				setCurrentPath(path) {
					state.currentPath = path;
				},
				setEntries(value) {
					state.entries = value;
				},
				getCurrentPath() {
					return state.currentPath;
				},
				getUploadFiles() {
					return state.uploadFiles;
				},
				setUploadFiles(files) {
					state.uploadFiles = files;
				},
				setUploading(value) {
					state.uploading = value;
				},
				setSystemState(val) {
					state.systemState = val;
				}
			}
		});

	async function ensureEntryType(entry: FileEntry): Promise<'file' | 'directory'> {
		if (entry.type === 'unknown') {
			const isDir = await checkIfDirectory(entry.name);
			state.entries = state.entries.map((item) =>
				item.name === entry.name
					? {
							...item,
							type: isDir ? 'directory' : 'file'
						}
					: item
			);
			return isDir ? 'directory' : 'file';
		}

		return entry.type === 'directory' ? 'directory' : 'file';
	}

	async function openEntry(entry: FileEntry) {
		const type = await ensureEntryType(entry);
		if (type === 'directory') {
			await loadDirectory(entry.name);
		} else {
			await downloadEntry(entry);
		}
	}

	async function goToParent() {
		const next = parentPath(state.currentPath);
		await loadDirectory(next);
	}

	async function refresh() {
		const path = state.currentPath;
		await Promise.all([loadDirectory(path), refreshSystemState()]);
	}

	function formatEntrySize(entry: FileEntry): string {
		return baseFormatEntrySize(entry);
	}

	function getEntryLabel(entry: FileEntry): string {
		return baseGetEntryLabel(entry);
	}

	function canDeleteEntry(entry: FileEntry, canModify: boolean): boolean {
		return canModify && entry.name !== DEFAULT_PATH && !isFileManagerReadOnlyPath(entry.name);
	}

	return {
		get state() {
			return state;
		},
		loadDirectory,
		openEntry,
		downloadEntry,
		deleteEntry,
		handleUpload,
		refresh,
		goToParent,
		refreshSystemState,
		setUploadFiles,
		formatEntrySize,
		getEntryLabel,
		canDeleteEntry
	};
}
