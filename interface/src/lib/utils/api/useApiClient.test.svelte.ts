import { describe, expect, it, vi } from 'vitest';
import type { ApiClientOptions } from './apiClient';
import { useApiClient } from './useApiClient.svelte';

const { mockInvalidate, mockSession } = vi.hoisted(() => {
	const invalidate = vi.fn();
	const apiOptions: ApiClientOptions = {
		bearerToken: 'jwt-token',
		onUnauthorized: invalidate
	};

	return {
		mockInvalidate: invalidate,
		mockSession: {
			bearerToken: 'jwt-token',
			isAuthenticated: true,
			isAdmin: true,
			canRead: true,
			canManage: true,
			apiOptions,
			invalidate
		}
	};
});

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => mockSession
}));

class ExampleApiService {
	constructor(public options: ApiClientOptions) {}
}

describe('useApiClient', () => {
	it('exposes the shared options alias', () => {
		const cleanup = $effect.root(() => {
			const apiClient = useApiClient();

			expect(apiClient.options).toBe(mockSession.apiOptions);
		});

		cleanup();
	});

	it('creates services with the shared ApiClientOptions', () => {
		const cleanup = $effect.root(() => {
			const apiClient = useApiClient();
			const service = apiClient.createService(ExampleApiService);

			expect(service.options).toBe(mockSession.apiOptions);
		});

		cleanup();
	});

	it('keeps unauthorized handling delegated through the shared session', () => {
		const cleanup = $effect.root(() => {
			const apiClient = useApiClient();

			apiClient.options.onUnauthorized?.();

			expect(mockInvalidate).toHaveBeenCalledOnce();
		});

		cleanup();
	});
});
