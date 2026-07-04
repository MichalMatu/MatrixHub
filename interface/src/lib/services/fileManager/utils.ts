import type { SystemInformation } from '$lib/types/system/system';
import type { FileManagerSystemState } from './types';

const ROOT_PATH = '/';
const LITTLEFS_PREFIX = '/littlefs';
const PROTECTED_CONFIG_ROOT = '/config';
const PROTECTED_LOG_ROOT = '/data';
export const FILE_MANAGER_SDCARD_PATH = '/sdcard';

export type FileManagerPathAccess = 'list' | 'download' | 'upload' | 'remove';

export type FileManagerPathValidationResult =
	| { ok: true; path: string }
	| { ok: false; error: 'fs/invalid_path' | 'fs/path_forbidden' };

function trimPath(path: string): string {
	return path.trim();
}

function ensureLeadingSlash(path: string): string {
	return path.startsWith(ROOT_PATH) ? path : `${ROOT_PATH}${path}`;
}

function startsWithPathPrefix(path: string, prefix: string): boolean {
	return path === prefix || path.startsWith(`${prefix}/`);
}

function canonicalizePath(path: string, fallback: string): string | null {
	const candidate = ensureLeadingSlash(trimPath(path) || fallback || ROOT_PATH);
	const segments = candidate.split('/');
	const normalized: string[] = [];

	for (const segment of segments) {
		if (!segment || segment === '.') {
			continue;
		}
		if (segment === '..') {
			return null;
		}
		for (const ch of segment) {
			if (ch === '\\' || ch < ' ') {
				return null;
			}
		}
		normalized.push(segment);
	}

	return normalized.length ? `/${normalized.join('/')}` : ROOT_PATH;
}

function toNativePath(canonicalPath: string): string {
	if (canonicalPath === LITTLEFS_PREFIX) {
		return ROOT_PATH;
	}
	if (startsWithPathPrefix(canonicalPath, LITTLEFS_PREFIX)) {
		return canonicalPath.slice(LITTLEFS_PREFIX.length) || ROOT_PATH;
	}
	return canonicalPath;
}

export function canonicalizeFileManagerPath(
	path: string,
	fallback: string = ROOT_PATH
): FileManagerPathValidationResult {
	const canonicalPath = canonicalizePath(path, fallback);
	if (!canonicalPath) {
		return { ok: false, error: 'fs/invalid_path' };
	}

	return { ok: true, path: canonicalPath };
}

export function isFileManagerReadOnlyPath(path: string): boolean {
	const canonicalPath = canonicalizePath(path, ROOT_PATH);
	if (!canonicalPath) {
		return false;
	}

	return startsWithPathPrefix(toNativePath(canonicalPath), PROTECTED_CONFIG_ROOT);
}

export function isFileManagerUploadBlockedPath(path: string): boolean {
	const canonicalPath = canonicalizePath(path, ROOT_PATH);
	if (!canonicalPath) {
		return false;
	}

	const nativePath = toNativePath(canonicalPath);
	return (
		startsWithPathPrefix(nativePath, PROTECTED_CONFIG_ROOT) ||
		startsWithPathPrefix(nativePath, PROTECTED_LOG_ROOT)
	);
}

export function validateFileManagerPathAccess(
	path: string,
	access: FileManagerPathAccess,
	fallback: string = ROOT_PATH
): FileManagerPathValidationResult {
	const canonicalPathResult = canonicalizeFileManagerPath(path, fallback);
	if (!canonicalPathResult.ok) {
		return canonicalPathResult;
	}

	if (startsWithPathPrefix(canonicalPathResult.path, FILE_MANAGER_SDCARD_PATH)) {
		return { ok: false, error: 'fs/path_forbidden' };
	}

	if (access === 'upload' && isFileManagerUploadBlockedPath(canonicalPathResult.path)) {
		return { ok: false, error: 'fs/path_forbidden' };
	}

	if (access === 'remove' && isFileManagerReadOnlyPath(canonicalPathResult.path)) {
		return { ok: false, error: 'fs/path_forbidden' };
	}

	return canonicalPathResult;
}

export function createFileManagerSystemState(
	systemInformation: SystemInformation | null = null
): FileManagerSystemState {
	return { systemInformation };
}
