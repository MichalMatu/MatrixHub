import { user } from '$lib/stores/user';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { signIn, type SignInRequest } from '$lib/services/api/core/SecurityApiService';
import { ErrorService } from '$lib/services/ui/ErrorService';
import { openDefaultCredentialsWarning } from './defaultCredentialsWarning';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

const LOGIN_FAIL_ANIMATION_DURATION_MS = 1500;
const LOGIN_PENDING_FEEDBACK_MIN_MS = 400;
const NOTIFICATION_DURATION_MS = 5000;
const INVALID_ACCESS_TOKEN_ERROR_CODE = 'auth/invalid_access_token';
const SESSION_INIT_FAILED_ERROR_CODE = 'auth/session_init_failed';

export function useLogin(onSuccessFn?: () => (() => void) | undefined) {
	let username = $state('');
	let password = $state('');
	let loginFailed = $state(false);
	let isSubmitting = $state(false);
	let loginFailedResetTimer: ReturnType<typeof setTimeout> | null = null;

	function resetLoginFailedSoon() {
		if (loginFailedResetTimer !== null) {
			clearTimeout(loginFailedResetTimer);
		}
		loginFailed = true;
		loginFailedResetTimer = setTimeout(() => {
			loginFailed = false;
			loginFailedResetTimer = null;
		}, LOGIN_FAIL_ANIMATION_DURATION_MS);
	}

	async function ensureMinimumPendingFeedback(startedAtMs: number) {
		const elapsedMs = Date.now() - startedAtMs;
		const remainingMs = LOGIN_PENDING_FEEDBACK_MIN_MS - elapsedMs;
		if (remainingMs <= 0) return;

		await new Promise((resolve) => setTimeout(resolve, remainingMs));
	}

	async function signInUser(data: SignInRequest) {
		if (isSubmitting) return;

		isSubmitting = true;
		loginFailed = false;
		const startedAtMs = Date.now();

		try {
			const token = await signIn(data);
			await ensureMinimumPendingFeedback(startedAtMs);

			if (token) {
				const accessToken = typeof token.access_token === 'string' ? token.access_token.trim() : '';
				if (!accessToken) {
					throw new Error(INVALID_ACCESS_TOKEN_ERROR_CODE);
				}

				user.init(accessToken);
				if (!user.bearer_token) {
					throw new Error(SESSION_INIT_FAILED_ERROR_CODE);
				}

				const loggedUsername = user.username || data.username;
				notifications.success(
					m.toast_login_success({ username: loggedUsername }, { locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
				const onSuccess = onSuccessFn?.();
				if (onSuccess) onSuccess();
				if (data.username === 'admin' && data.password === 'admin') {
					openDefaultCredentialsWarning();
				}
			} else {
				username = '';
				password = '';
				notifications.error(
					m.toast_login_failed({ locale: i18n.languageTag }),
					NOTIFICATION_DURATION_MS
				);
				resetLoginFailedSoon();
			}
		} catch (error) {
			await ensureMinimumPendingFeedback(startedAtMs);
			ErrorService.handle(error, m.toast_login_error({ locale: i18n.languageTag }));
			resetLoginFailedSoon();
		} finally {
			isSubmitting = false;
		}
	}

	return {
		get username() {
			return username;
		},
		set username(value: string) {
			username = value;
		},
		get password() {
			return password;
		},
		set password(value: string) {
			password = value;
		},
		get loginFailed() {
			return loginFailed;
		},
		get isSubmitting() {
			return isSubmitting;
		},
		get authNotice() {
			return user.authNotice;
		},
		signInUser
	};
}
