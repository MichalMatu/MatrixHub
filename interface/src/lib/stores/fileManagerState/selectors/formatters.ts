import { type FileEntry } from '$lib/services/fileManager';
import { basename } from '$lib/utils/path/helpers';
import { formatBytes } from '$lib/utils';

export function formatEntrySize(entry: FileEntry): string {
	if (entry.type === 'directory') {
		return '—';
	}

	if (entry.size === 0) {
		return '0 B';
	}

	return formatBytes(entry.size);
}

export function getEntryLabel(entry: FileEntry): string {
	return basename(entry.name);
}
