import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import type { ApiClientOptions } from '$lib/utils/api/apiClient';

/**
 * Composable for creating API service instances with consistent configuration.
 *
 * Automatically derives API client options from current user authentication state,
 * eliminating repetitive boilerplate across components.
 *
 * Features:
 * - Reactive to authentication changes (user.bearer_token)
 * - Type-safe service creation
 * - Consistent error handling configuration
 *
 * @example
 * ```typescript
 * // In a Svelte component
 * const { createService } = useApiClient();
 *
 * // Create API service (reactive)
 * const api = $derived(createService(SensorsApiService));
 *
 * // Use in async functions
 * const data = await api.getData();
 * ```
 */
export function useApiClient() {
	const session = useSessionAccess();

	/**
	 * Get current API client options from app state
	 */
	function getOptions(): ApiClientOptions {
		return session.apiOptions;
	}

	/**
	 * Create an API service instance with current options
	 *
	 * @param ServiceClass - The API service class constructor
	 * @returns Configured service instance
	 *
	 * @example
	 * ```typescript
	 * const alarmsApi = createService(AlarmsApiService);
	 * const sensorsApi = createService(SensorsApiService);
	 * ```
	 */
	function createService<T>(ServiceClass: new (opts: ApiClientOptions) => T): T {
		return new ServiceClass(getOptions());
	}

	return {
		/** Current API client options (reactive) */
		get options() {
			return getOptions();
		},
		/** Create a typed API service instance */
		createService
	};
}
