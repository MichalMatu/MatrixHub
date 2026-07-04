import { describe, expect, it } from 'vitest';
import {
	canonicalizeFileManagerPath,
	isFileManagerUploadBlockedPath,
	isFileManagerReadOnlyPath,
	validateFileManagerPathAccess
} from './utils';

describe('fileManager utils', () => {
	it('canonicalizes duplicate slashes and relative roots', () => {
		expect(canonicalizeFileManagerPath('data//logs/')).toEqual({
			ok: true,
			path: '/data/logs'
		});
	});

	it('rejects traversal attempts', () => {
		expect(canonicalizeFileManagerPath('/littlefs/../config')).toEqual({
			ok: false,
			error: 'fs/invalid_path'
		});
		expect(canonicalizeFileManagerPath('/foo/..')).toEqual({
			ok: false,
			error: 'fs/invalid_path'
		});
	});

	it('keeps protected config paths visible for read operations', () => {
		expect(canonicalizeFileManagerPath('/littlefs/config')).toEqual({
			ok: true,
			path: '/littlefs/config'
		});
		expect(isFileManagerReadOnlyPath('/littlefs/config')).toBe(true);
		expect(validateFileManagerPathAccess('/littlefs/config', 'list')).toEqual({
			ok: true,
			path: '/littlefs/config'
		});
		expect(validateFileManagerPathAccess('/littlefs/config', 'download')).toEqual({
			ok: true,
			path: '/littlefs/config'
		});
	});

	it('blocks writes to protected config paths', () => {
		expect(validateFileManagerPathAccess('//config', 'upload')).toEqual({
			ok: false,
			error: 'fs/path_forbidden'
		});
		expect(validateFileManagerPathAccess('/littlefs/config', 'remove')).toEqual({
			ok: false,
			error: 'fs/path_forbidden'
		});
	});

	it('blocks uploads to log storage while keeping log downloads and removals client-valid', () => {
		expect(isFileManagerUploadBlockedPath('/data')).toBe(true);
		expect(isFileManagerUploadBlockedPath('/littlefs/data/2026-06')).toBe(true);
		expect(isFileManagerReadOnlyPath('/data')).toBe(false);
		expect(validateFileManagerPathAccess('/data', 'upload')).toEqual({
			ok: false,
			error: 'fs/path_forbidden'
		});
		expect(validateFileManagerPathAccess('/data/2026-06/log.bin', 'download')).toEqual({
			ok: true,
			path: '/data/2026-06/log.bin'
		});
		expect(validateFileManagerPathAccess('/data/old.bin', 'remove')).toEqual({
			ok: true,
			path: '/data/old.bin'
		});
	});

	it('uses the provided fallback for empty directories', () => {
		expect(canonicalizeFileManagerPath('', '/sdcard')).toEqual({
			ok: true,
			path: '/sdcard'
		});
	});
});
