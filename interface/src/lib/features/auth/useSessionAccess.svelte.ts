import { user, type UserSessionNotice } from '$lib/stores/user';
import type { ApiClientOptions } from '$lib/utils/api/apiClient';

export interface SessionAccess {
	readonly bearerToken: string;
	readonly isAuthenticated: boolean;
	readonly isAdmin: boolean;
	readonly canRead: boolean;
	readonly canManage: boolean;
	readonly username: string;
	readonly apiOptions: ApiClientOptions;
	invalidate(reason?: UserSessionNotice | null): void;
}

export function useSessionAccess(): SessionAccess {
	const bearerToken = $derived(user.bearer_token);
	const isAuthenticated = $derived(user.isValid);
	const isAdmin = $derived(isAuthenticated && user.admin);
	const canRead = $derived(isAuthenticated);
	const canManage = $derived(isAuthenticated && isAdmin);
	const username = $derived(user.username);

	function invalidate(reason: UserSessionNotice | null = null) {
		user.invalidate(reason);
	}

	return {
		get bearerToken() {
			return bearerToken;
		},
		get isAuthenticated() {
			return isAuthenticated;
		},
		get isAdmin() {
			return isAdmin;
		},
		get canRead() {
			return canRead;
		},
		get canManage() {
			return canManage;
		},
		get username() {
			return username;
		},
		get apiOptions(): ApiClientOptions {
			return {
				bearerToken,
				onUnauthorized: () => invalidate('unauthorized')
			};
		},
		invalidate
	};
}
