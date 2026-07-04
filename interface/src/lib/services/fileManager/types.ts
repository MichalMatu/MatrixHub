import type { SystemInformation } from '$lib/types/system/system';
import type { ApiClient } from '$lib/utils';

export type EntryType = 'file' | 'directory' | 'unknown';

export type RawFileEntry = {
	name: string;
	size: number;
	directory?: boolean;
};

export type FileEntry = RawFileEntry & {
	type: EntryType;
};

export type FileManagerSystemState = {
	systemInformation: SystemInformation | null;
};

export type FileManagerActionCallbacks = {
	setIsLoading(value: boolean): void;
	setErrorMessage(message: string | null): void;
	setCurrentPath(path: string): void;
	setEntries(entries: FileEntry[]): void;
	getCurrentPath(): string;
	getUploadFiles(): FileList | null;
	setUploadFiles(files: FileList | null): void;
	setUploading(value: boolean): void;
	setSystemState(state: FileManagerSystemState): void;
};

export interface CreateFileManagerActionsOptions {
	defaultPath: string;
	directoryCache: Map<string, boolean>;
	apiClient: ApiClient;
	checkIfDirectory(path: string): Promise<boolean>;
	callbacks: FileManagerActionCallbacks;
}
