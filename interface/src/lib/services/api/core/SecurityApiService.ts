import {
	ApiError,
	createApiClient,
	getRequestFailureKind,
	type ApiClientOptions
} from '$lib/utils';

const ENDPOINT_SECURITY_SETTINGS = '/rest/securitySettings';
const ENDPOINT_VERIFY_AUTHORIZATION = '/rest/verifyAuthorization';
const ENDPOINT_SIGN_IN = '/rest/signIn';

export interface UserSetting {
	username: string;
	password: string;
	admin: boolean;
}

export interface SecuritySettings {
	jwt_secret: string;
	jwt_secret_configured?: boolean;
	users: UserSetting[];
}

export interface SignInRequest {
	username: string;
	password: string;
}

interface SignInResponse {
	access_token: string;
}

/**
 * SecurityApiService - Handles authentication and security settings
 *
 * Provides methods for user authentication, JWT token management,
 * and security configuration via REST API.
 */
export class SecurityApiService {
	private client;
	private static readonly GET_SETTINGS_TIMEOUT_MS = 15000;
	private static readonly SAVE_SETTINGS_TIMEOUT_MS = 20000;
	private static readonly VERIFY_TIMEOUT_MS = 15000;

	/**
	 * Creates a new SecurityApiService instance
	 * @param options API client configuration with security settings
	 */
	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	/**
	 * Retrieves current security settings from the device.
	 * The JWT secret is write-only; a blank value means "keep existing secret".
	 * @throws ApiError if request fails or times out
	 */
	async getSecuritySettings(): Promise<SecuritySettings> {
		return this.client.get<SecuritySettings>(ENDPOINT_SECURITY_SETTINGS, {
			signal: AbortSignal.timeout(SecurityApiService.GET_SETTINGS_TIMEOUT_MS)
		});
	}

	/**
	 * Saves updated security settings to the device
	 * @param settings New security configuration
	 * @returns Promise resolving to saved settings
	 * @throws ApiError if request fails or times out
	 */
	async saveSecuritySettings(settings: SecuritySettings): Promise<SecuritySettings> {
		return this.client.post<SecuritySettings>(ENDPOINT_SECURITY_SETTINGS, settings, {
			signal: AbortSignal.timeout(SecurityApiService.SAVE_SETTINGS_TIMEOUT_MS)
		});
	}

	/**
	 * Verifies if current authentication token is valid
	 * @returns Promise resolving to true if authorized, false otherwise
	 */
	async verifyAuthorization(): Promise<boolean> {
		try {
			const res = await this.client.fetch(ENDPOINT_VERIFY_AUTHORIZATION, {
				method: 'GET',
				signal: AbortSignal.timeout(SecurityApiService.VERIFY_TIMEOUT_MS)
			});
			return res.status === 200;
		} catch {
			return false;
		}
	}
}

// Standalone sign-in function (doesn't require auth)
export async function signIn(data: SignInRequest): Promise<SignInResponse | null> {
	const publicClient = createApiClient({ bearerToken: '' });
	const SIGN_IN_TIMEOUT_MS = 15000;
	const SIGN_IN_RETRY_DELAY_MS = 300;
	const MAX_RETRY_ATTEMPTS = 1;

	async function attemptSignIn() {
		return publicClient.post<SignInResponse>(ENDPOINT_SIGN_IN, data, {
			signal: AbortSignal.timeout(SIGN_IN_TIMEOUT_MS)
		});
	}

	for (let attempt = 0; attempt <= MAX_RETRY_ATTEMPTS; attempt++) {
		try {
			return await attemptSignIn();
		} catch (error) {
			// Invalid credentials are expected user input; don't spam the console as an "error".
			if (
				error instanceof ApiError &&
				error.status === 401 &&
				(!error.errorCode || error.errorCode === 'auth/invalid_credentials')
			) {
				return null;
			}

			const failureKind = getRequestFailureKind(error);
			const shouldRetry =
				attempt < MAX_RETRY_ATTEMPTS && (failureKind === 'timeout' || failureKind === 'network');
			if (shouldRetry) {
				await new Promise((resolve) => setTimeout(resolve, SIGN_IN_RETRY_DELAY_MS));
				continue;
			}

			throw error;
		}
	}

	throw new Error('auth/sign_in_unreachable');
}
