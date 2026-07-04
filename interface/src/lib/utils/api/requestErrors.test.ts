import { describe, it, expect } from 'vitest';
import { ApiError } from './apiClient';
import {
	getRequestAbortKind,
	getRequestFailureKind,
	isAbortLike,
	isFailureLike,
	isNetworkLike,
	isTimeoutLike,
	toUserRequestErrorMessage
} from './requestErrors';

describe('requestErrors', () => {
	it('detects AbortError as abort', () => {
		const err = new DOMException('The operation was aborted.', 'AbortError');
		expect(getRequestAbortKind(err)).toBe('abort');
		expect(isAbortLike(err)).toBe(true);
		expect(isTimeoutLike(err)).toBe(false);
	});

	it('detects TimeoutError as timeout', () => {
		const err = new DOMException('signal timed out', 'TimeoutError');
		expect(getRequestAbortKind(err)).toBe('timeout');
		expect(isAbortLike(err)).toBe(true);
		expect(isTimeoutLike(err)).toBe(true);
	});

	it('formats timeout message', () => {
		const err = new DOMException('signal timed out', 'TimeoutError');
		expect(toUserRequestErrorMessage(err)).toBe('Request timed out.');
	});

	it('detects network-like errors', () => {
		const err = new TypeError('Failed to fetch');
		expect(isNetworkLike(err)).toBe(true);
		expect(getRequestFailureKind(err)).toBe('network');
		expect(isFailureLike(err)).toBe(true);
		expect(toUserRequestErrorMessage(err)).toBe('Network error. Device may be unreachable.');
	});

	it('uses error message for normal errors', () => {
		const err = new Error('Boom');
		expect(toUserRequestErrorMessage(err)).toBe('Boom');
	});

	it('prefers ApiError errorCode mappings over raw server messages', () => {
		const err = new ApiError(
			429,
			'POST /rest/signIn failed',
			'Too Many Login Attempts',
			'auth/login_rate_limited'
		);
		expect(toUserRequestErrorMessage(err)).toBe(
			'Too many login attempts. Please wait a moment and try again.'
		);
	});

	it('maps transactional save failure codes to user-facing messages', () => {
		expect(toUserRequestErrorMessage(new Error('auth/invalid_credentials'))).toBe(
			'Wrong username or password.'
		);
		expect(toUserRequestErrorMessage(new Error('auth/missing_credentials'))).toBe(
			'Username and password are required.'
		);
		expect(toUserRequestErrorMessage(new Error('auth/rate_limited'))).toBe(
			'Too many requests. Please try again in a moment.'
		);
		expect(toUserRequestErrorMessage(new Error('auth/session_expired'))).toBe('Sign in required.');
		expect(toUserRequestErrorMessage(new Error('auth/unauthorized'))).toBe('Sign in required.');
		expect(toUserRequestErrorMessage(new Error('auth/forbidden'))).toBe(
			'You do not have permission to perform this action.'
		);
		expect(toUserRequestErrorMessage(new Error('auth/invalid_access_token'))).toBe(
			'The device returned an invalid session token.'
		);
		expect(toUserRequestErrorMessage(new Error('auth/session_init_failed'))).toBe(
			'Could not start your session. Please try signing in again.'
		);
		expect(toUserRequestErrorMessage(new Error('input/invalid_json'))).toBe(
			'The device rejected the request because the payload was invalid.'
		);
		expect(toUserRequestErrorMessage(new Error('input/payload_too_large'))).toBe(
			'The request was too large for the device to process.'
		);
		expect(toUserRequestErrorMessage(new Error('config/save_failed'))).toBe(
			'The device could not save the settings.'
		);
		expect(toUserRequestErrorMessage(new Error('config/apply_failed'))).toBe(
			'The device could not apply the settings.'
		);
		expect(toUserRequestErrorMessage(new Error('rtc/restore_failed'))).toBe(
			'The device could not restore the previous settings after the failed update.'
		);
		expect(toUserRequestErrorMessage(new Error('internal/update_failed'))).toBe(
			'The device could not apply the settings.'
		);
		expect(toUserRequestErrorMessage(new Error('input/alarm_rules_duplicate_id'))).toBe(
			'Alarm rules contain invalid or incomplete data.'
		);
		expect(toUserRequestErrorMessage(new Error('input/alarm_rules_duplicate_name'))).toBe(
			'Alarm rule names must be unique.'
		);
		expect(toUserRequestErrorMessage(new Error('input/alarm_rules_too_many'))).toBe(
			'Too many alarm rules. Remove a rule before saving.'
		);
		expect(toUserRequestErrorMessage(new Error('input/boot_script_not_found'))).toBe(
			'The selected boot script does not exist.'
		);
		expect(toUserRequestErrorMessage(new Error('fs/invalid_path'))).toBe(
			'Invalid or unsafe file path.'
		);
		expect(toUserRequestErrorMessage(new Error('fs/path_forbidden'))).toBe(
			'This path is protected in File Manager. The requested operation is blocked, but browsing and downloads may still be available.'
		);
		expect(toUserRequestErrorMessage(new Error('fs/path_missing'))).toBe('Missing file path.');
		expect(toUserRequestErrorMessage(new Error('fs/file_not_found'))).toBe('File not found.');
		expect(toUserRequestErrorMessage(new Error('fs/open_failed'))).toBe('Failed to open file.');
		expect(toUserRequestErrorMessage(new Error('fs/delete_failed'))).toBe(
			'Failed to delete file or folder.'
		);
		expect(toUserRequestErrorMessage(new Error('fs/upload_failed'))).toBe('File upload failed.');
		expect(toUserRequestErrorMessage(new Error('fs/already_exists'))).toBe(
			'A file with this name already exists.'
		);
		expect(toUserRequestErrorMessage(new Error('fs/storage_full'))).toBe(
			'Insufficient storage space.'
		);
		expect(toUserRequestErrorMessage(new Error('logs/active_file'))).toBe(
			"Can't remove today's active log while logging is running."
		);
		expect(toUserRequestErrorMessage(new Error('busy/filesystem'))).toBe(
			'Filesystem is busy, try again.'
		);
		expect(toUserRequestErrorMessage(new Error('internal/out_of_memory'))).toBe(
			'Device ran out of memory.'
		);
		expect(toUserRequestErrorMessage(new Error('usb_terminal/session_active'))).toBe(
			'Stop the active terminal session before disabling USB Terminal.'
		);
	});

	it('falls back for unknown objects', () => {
		const err = { whatever: true };
		expect(toUserRequestErrorMessage(err)).toBe('Request failed.');
	});
});
