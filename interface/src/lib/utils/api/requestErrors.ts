import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

export type RequestAbortKind = 'abort' | 'timeout';

export type RequestFailureKind = RequestAbortKind | 'network';

function resolveKnownBackendErrorCode(message: string): string | null {
	switch (message) {
		case 'auth/invalid_credentials':
			return m.auth_error_invalid_credentials({ locale: i18n.languageTag });
		case 'auth/missing_credentials':
			return m.auth_error_missing_credentials({ locale: i18n.languageTag });
		case 'auth/rate_limited':
			return m.auth_error_rate_limited({ locale: i18n.languageTag });
		case 'auth/login_rate_limited':
			return m.auth_error_login_rate_limited({ locale: i18n.languageTag });
		case 'auth/session_expired':
			// Keep backwards compatibility with older firmware responses, but surface
			// the simplified frontend state as a generic unauthorized session.
			return m.auth_error_unauthorized({ locale: i18n.languageTag });
		case 'auth/unauthorized':
			return m.auth_error_unauthorized({ locale: i18n.languageTag });
		case 'auth/forbidden':
			return m.auth_error_forbidden({ locale: i18n.languageTag });
		case 'auth/invalid_access_token':
			return m.auth_error_invalid_access_token({ locale: i18n.languageTag });
		case 'auth/session_init_failed':
			return m.auth_error_session_init_failed({ locale: i18n.languageTag });
		case 'input/invalid_json':
			return m.settings_error_invalid_request({ locale: i18n.languageTag });
		case 'input/payload_too_large':
			return m.settings_error_request_too_large({ locale: i18n.languageTag });
		case 'config/save_failed':
			return m.settings_error_persist_failed({ locale: i18n.languageTag });
		case 'config/apply_failed':
			return m.settings_error_update_failed({ locale: i18n.languageTag });
		case 'rtc/restore_failed':
			return m.settings_error_restore_failed({ locale: i18n.languageTag });
		case 'internal/update_failed':
			return m.settings_error_update_failed({ locale: i18n.languageTag });
		case 'input/alarm_rules_invalid_schema':
		case 'input/alarm_rules_missing_rules':
		case 'input/alarm_rules_invalid_rule':
		case 'input/alarm_rules_duplicate_id':
			return m.alarms_error_invalid_rules({ locale: i18n.languageTag });
		case 'input/alarm_rules_duplicate_name':
			return m.alarms_error_duplicate_name({ locale: i18n.languageTag });
		case 'input/alarm_rules_too_many':
			return m.alarms_error_too_many_rules({ locale: i18n.languageTag });
		case 'input/boot_script_not_found':
			return m.macros_error_boot_script_not_found({ locale: i18n.languageTag });
		case 'fs/invalid_path':
			return m.file_manager_error_invalid_path({ locale: i18n.languageTag });
		case 'fs/path_forbidden':
		case 'fs/config_write_forbidden':
			return m.file_manager_error_path_forbidden({ locale: i18n.languageTag });
		case 'fs/path_missing':
			return m.file_manager_error_path_missing({ locale: i18n.languageTag });
		case 'fs/file_not_found':
			return m.file_manager_error_file_not_found({ locale: i18n.languageTag });
		case 'fs/open_failed':
			return m.file_manager_error_open_failed({ locale: i18n.languageTag });
		case 'fs/delete_failed':
			return m.file_manager_error_delete_failed({ locale: i18n.languageTag });
		case 'fs/upload_failed':
			return m.file_manager_error_upload_failed({ locale: i18n.languageTag });
		case 'fs/already_exists':
			return m.file_manager_error_already_exists({ locale: i18n.languageTag });
		case 'fs/storage_full':
			return m.file_manager_error_storage_full({ locale: i18n.languageTag });
		case 'logs/active_file':
			return m.logs_delete_active({ locale: i18n.languageTag });
		case 'busy/filesystem':
			return m.file_manager_error_filesystem_busy({ locale: i18n.languageTag });
		case 'internal/out_of_memory':
			return m.file_manager_error_out_of_memory({ locale: i18n.languageTag });
		case 'usb_terminal/session_active':
			return m.usb_terminal_disable_requires_cancel({ locale: i18n.languageTag });
		case 'keyboard/disabled':
			return m.keyboard_error_disabled({ locale: i18n.languageTag });
		default:
			return null;
	}
}

function getErrorName(error: unknown): string | null {
	if (
		error &&
		typeof error === 'object' &&
		'name' in error &&
		typeof (error as { name: unknown }).name === 'string'
	) {
		return (error as { name: string }).name;
	}
	return null;
}

function getErrorMessage(error: unknown): string {
	if (error instanceof Error) return error.message;
	if (typeof error === 'string') return error;
	try {
		return JSON.stringify(error);
	} catch {
		return String(error);
	}
}

function getErrorCode(error: unknown): string | null {
	if (
		error &&
		typeof error === 'object' &&
		'errorCode' in error &&
		typeof (error as { errorCode: unknown }).errorCode === 'string'
	) {
		return (error as { errorCode: string }).errorCode;
	}
	return null;
}

export function getRequestAbortKind(error: unknown): RequestAbortKind | null {
	const name = getErrorName(error);
	const message = getErrorMessage(error).toLowerCase();

	// Fetch + AbortSignal.timeout often yields TimeoutError (Chromium) or AbortError.
	if (name === 'TimeoutError') return 'timeout';

	if (name === 'AbortError') {
		if (message.includes('timeout') || message.includes('timed out')) return 'timeout';
		return 'abort';
	}

	// Some environments wrap aborts into plain Errors.
	if (message.includes('aborted')) return 'abort';
	if (message.includes('timeout') || message.includes('timed out')) return 'timeout';

	return null;
}

export function isNetworkLike(error: unknown): boolean {
	// Fetch network failures in browsers are typically TypeError("Failed to fetch")
	// or a variant like "NetworkError" / "Load failed".
	const name = getErrorName(error);
	const message = getErrorMessage(error).toLowerCase();
	if (name === 'TypeError') {
		return (
			message.includes('failed to fetch') ||
			message.includes('networkerror') ||
			message.includes('load failed') ||
			message.includes('the network connection was lost')
		);
	}
	return (
		message.includes('failed to fetch') ||
		message.includes('networkerror') ||
		message.includes('load failed') ||
		message.includes('the network connection was lost')
	);
}

export function getRequestFailureKind(error: unknown): RequestFailureKind | null {
	const abortKind = getRequestAbortKind(error);
	if (abortKind) return abortKind;
	if (isNetworkLike(error)) return 'network';
	return null;
}

export function isAbortLike(error: unknown): boolean {
	return getRequestAbortKind(error) !== null;
}

export function isTimeoutLike(error: unknown): boolean {
	return getRequestAbortKind(error) === 'timeout';
}

export function isFailureLike(error: unknown): boolean {
	return getRequestFailureKind(error) !== null;
}

function toDisplayError(error: unknown): string {
	if (error instanceof Error) return error.message;
	return String(error);
}

export function toUserRequestErrorMessage(
	error: unknown,
	options: {
		timeoutMessage?: string;
		abortMessage?: string;
		networkMessage?: string;
		fallbackMessage?: string;
	} = {}
): string {
	const {
		timeoutMessage = m.toast_request_timeout({ locale: i18n.languageTag }),
		abortMessage = m.restart_error_cancelled({ locale: i18n.languageTag }),
		networkMessage = m.request_error_network({ locale: i18n.languageTag }),
		fallbackMessage = m.request_error_failed({ locale: i18n.languageTag })
	} = options;

	const kind = getRequestAbortKind(error);
	if (kind === 'timeout') return timeoutMessage;
	if (kind === 'abort') return abortMessage;
	if (isNetworkLike(error)) return networkMessage;

	const errorCode = getErrorCode(error);
	const mappedByCode = errorCode ? resolveKnownBackendErrorCode(errorCode) : null;
	if (mappedByCode) return mappedByCode;

	const msg = toDisplayError(error);
	const mappedMessage = resolveKnownBackendErrorCode(msg);
	if (mappedMessage) return mappedMessage;

	return msg && msg !== '[object Object]' ? msg : fallbackMessage;
}
