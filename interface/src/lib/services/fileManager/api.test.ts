import { beforeEach, describe, expect, it, vi } from 'vitest';
import { createFileManagerActions, fetchFileManagerSystemState } from './api';
import type { FileEntry, FileManagerSystemState } from './types';

const notificationMocks = vi.hoisted(() => ({
	showError: vi.fn(),
	showSuccess: vi.fn()
}));

const systemStatusMocks = vi.hoisted(() => ({
	getSnapshot: vi.fn<() => unknown | null>(() => null)
}));

vi.mock('$lib/utils/ui/notifications', () => ({
	showError: notificationMocks.showError,
	showSuccess: notificationMocks.showSuccess
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: {
		getSnapshot: systemStatusMocks.getSnapshot
	}
}));

function createHarness() {
	const state: {
		currentPath: string;
		entries: FileEntry[];
		errorMessage: string | null;
		isLoading: boolean;
		uploadFiles: FileList | null;
		uploading: boolean;
		systemState: FileManagerSystemState;
	} = {
		currentPath: '/',
		entries: [],
		errorMessage: null,
		isLoading: false,
		uploadFiles: null,
		uploading: false,
		systemState: {
			systemInformation: null
		}
	};

	const apiClient = {
		get: vi.fn(),
		delete: vi.fn(),
		fetch: vi.fn()
	};

	const actions = createFileManagerActions(
		{
			defaultPath: '/',
			directoryCache: new Map<string, boolean>(),
			checkIfDirectory: vi.fn(async () => false),
			apiClient: apiClient as never,
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
				setEntries(entries) {
					state.entries = entries;
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
				setSystemState(next) {
					state.systemState = next;
				}
			}
		},
		{
			fetchSystemState: vi.fn(async () => state.systemState)
		}
	);

	return { actions, apiClient, state };
}

describe('fileManager api', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('reuses cached system_status snapshot for file manager system state', async () => {
		systemStatusMocks.getSnapshot.mockReturnValue({
			system_info: {
				fs_total: 1024,
				fs_used: 256
			}
		});
		const apiClient = {
			get: vi.fn()
		};

		const state = await fetchFileManagerSystemState(apiClient as never);

		expect(systemStatusMocks.getSnapshot).toHaveBeenCalledWith('system_status');
		expect(apiClient.get).not.toHaveBeenCalled();
		expect(state.systemInformation?.fs_total).toBe(1024);
	});

	it('does not send a list request for an invalid path', async () => {
		const { actions, apiClient, state } = createHarness();

		await actions.loadDirectory('/littlefs/../config');

		expect(apiClient.get).not.toHaveBeenCalled();
		expect(state.errorMessage).toBe('Invalid or unsafe file path.');
	});

	it('keeps protected entries visible in the loaded list', async () => {
		const { actions, apiClient, state } = createHarness();
		apiClient.get.mockResolvedValue({
			files: [
				{ name: '/config', size: 0, directory: true },
				{ name: '/data', size: 0, directory: true }
			]
		});

		await actions.loadDirectory('/');

		expect(state.entries.map((entry) => entry.name)).toEqual(['/config', '/data']);
	});

	it('uploads using only the canonical path parameter', async () => {
		const { actions, apiClient, state } = createHarness();
		const uploadFile = new File(['hello'], 'hello.txt', { type: 'text/plain' });
		const fileList = {
			0: uploadFile,
			length: 1,
			item: (index: number) => (index === 0 ? uploadFile : null)
		} as unknown as FileList;

		state.currentPath = '/uploads';
		state.uploadFiles = fileList;
		apiClient.fetch.mockResolvedValue(new Response(JSON.stringify({ ok: true }), { status: 200 }));

		await actions.handleUpload();

		expect(apiClient.fetch).toHaveBeenCalled();
		const [url] = apiClient.fetch.mock.calls[0] as [string, RequestInit];
		expect(url).toBe('/rest/fs/upload?path=%2Fuploads');
		expect(url).not.toContain('backend=');
	});

	it('blocks uploads to log storage before opening an HTTP request', async () => {
		const { actions, apiClient, state } = createHarness();
		const uploadFile = new File(['probe'], 'probe.txt', { type: 'text/plain' });
		const fileList = {
			0: uploadFile,
			length: 1,
			item: (index: number) => (index === 0 ? uploadFile : null)
		} as unknown as FileList;

		state.currentPath = '/data';
		state.uploadFiles = fileList;

		await actions.handleUpload();

		expect(apiClient.fetch).not.toHaveBeenCalled();
		expect(notificationMocks.showError).toHaveBeenCalled();
		expect(state.uploading).toBe(false);
	});
});
