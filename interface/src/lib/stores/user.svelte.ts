import { goto } from '$app/navigation';
import { getTokenPayload } from '$lib/utils/auth/token';

export type UserSessionNotice = 'unauthorized';

const USER_STORAGE_KEY = 'user';
const WS_ACCESS_TOKEN_COOKIE = 'access_token';
const WS_ACCESS_TOKEN_COOKIE_PATH = '/ws';
const LEGACY_WS_ACCESS_TOKEN_COOKIE_PATH = '/';

function isSecureCookieContext() {
	return globalThis.location?.protocol === 'https:';
}

function buildWsAccessTokenCookieAttrs(path: string) {
	return `path=${path}; SameSite=Strict${isSecureCookieContext() ? '; Secure' : ''}`;
}

class UserStore {
	username = $state('');
	admin = $state(false);
	bearer_token = $state('');
	authNotice = $state<UserSessionNotice | null>(null);

	/**
	 * Returns true if a session token is currently present.
	 * Backend authorization is the source of truth; the browser no longer
	 * maintains a separate expiry model.
	 */
	get isValid() {
		return !!this.bearer_token;
	}

	constructor() {
		if (typeof window !== 'undefined') {
			const stored = localStorage.getItem(USER_STORAGE_KEY);
			if (stored) {
				try {
					const parsed = JSON.parse(stored);
					this.restoreSession(parsed.bearer_token || '', parsed.username || '', !!parsed.admin);
				} catch (e) {
					console.error('Failed to parse stored user', e);
					localStorage.removeItem(USER_STORAGE_KEY);
				}
			}
		}
	}

	init(accessToken: string) {
		const decoded = getTokenPayload(accessToken);
		if (!decoded?.username) {
			console.error('Invalid token');
			return;
		}

		this.username = decoded.username;
		this.admin = !!decoded.admin;
		this.bearer_token = accessToken;
		this.authNotice = null;
		this.syncWsAuthCookie(accessToken);
		this.persist();
	}

	invalidate(reason: UserSessionNotice | null = null) {
		const hadSession = !!(this.username || this.admin || this.bearer_token);
		this.authNotice = reason;
		this.clearSession();
		if (!hadSession) return;
		// Redirect to login
		goto('/');
	}

	clearAuthNotice() {
		this.authNotice = null;
	}

	private restoreSession(accessToken: string, username: string, admin: boolean) {
		if (!accessToken) return;
		const decoded = getTokenPayload(accessToken);
		if (!decoded?.username) {
			this.clearSession();
			return;
		}

		// On restore we only require a decodable identity claim. Any revoked or
		// invalid session is rejected later by normal 401 handling.
		this.username = decoded.username ?? username;
		this.admin = decoded.admin ?? admin;
		this.bearer_token = accessToken;
		this.syncWsAuthCookie(accessToken);
	}

	private clearSession() {
		this.username = '';
		this.admin = false;
		this.bearer_token = '';
		this.clearWsAuthCookie();
		if (typeof window !== 'undefined') {
			localStorage.removeItem(USER_STORAGE_KEY);
		}
	}

	private persist() {
		if (typeof window !== 'undefined') {
			localStorage.setItem(
				USER_STORAGE_KEY,
				JSON.stringify({
					username: this.username,
					admin: this.admin,
					bearer_token: this.bearer_token
				})
			);
		}
	}

	private syncWsAuthCookie(accessToken: string) {
		if (typeof document === 'undefined') return;

		this.clearLegacyWsAuthCookie();
		const cookieAttrs = buildWsAccessTokenCookieAttrs(WS_ACCESS_TOKEN_COOKIE_PATH);
		// Mirror the access token for WS auth without deriving a local Max-Age from
		// JWT contents; the device decides when the session stops being valid.
		document.cookie = `${WS_ACCESS_TOKEN_COOKIE}=${encodeURIComponent(accessToken)}; ${cookieAttrs}`;
	}

	private clearWsAuthCookie() {
		if (typeof document === 'undefined') return;
		document.cookie = `${WS_ACCESS_TOKEN_COOKIE}=; Max-Age=0; ${buildWsAccessTokenCookieAttrs(WS_ACCESS_TOKEN_COOKIE_PATH)}`;
		this.clearLegacyWsAuthCookie();
	}

	private clearLegacyWsAuthCookie() {
		if (typeof document === 'undefined') return;
		document.cookie = `${WS_ACCESS_TOKEN_COOKIE}=; Max-Age=0; ${buildWsAccessTokenCookieAttrs(LEGACY_WS_ACCESS_TOKEN_COOKIE_PATH)}`;
	}
}

export const user = new UserStore();
