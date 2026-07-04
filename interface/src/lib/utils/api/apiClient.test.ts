import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { createApiClient, getApiHeaders } from './apiClient';

describe('apiClient', () => {
	const originalFetch = globalThis.fetch;

	beforeEach(() => {
		globalThis.fetch = vi.fn(async () => {
			return new Response(JSON.stringify({ ok: true }), {
				status: 200,
				headers: { 'content-type': 'application/json' }
			});
		}) as unknown as typeof fetch;
	});

	afterEach(() => {
		globalThis.fetch = originalFetch;
		vi.restoreAllMocks();
	});

	it('omits Authorization header when token is empty', () => {
		const headers = getApiHeaders({ bearerToken: '' });
		expect(headers).not.toHaveProperty('Authorization');
		expect(headers['Content-Type']).toBe('application/json');
	});

	it('sets Authorization header when token is present', () => {
		const headers = getApiHeaders({ bearerToken: 'abc' });
		expect(headers.Authorization).toBe('Bearer abc');
	});

	it('does not set Authorization header when token is empty', () => {
		const headers = getApiHeaders({ bearerToken: '' });
		expect(headers).not.toHaveProperty('Authorization');
	});

	it('createApiClient.get uses headers without Authorization when token is empty', async () => {
		const client = createApiClient({ bearerToken: '' });
		await client.get('/rest/features');

		const fetchMock = globalThis.fetch as unknown as ReturnType<typeof vi.fn>;
		expect(fetchMock).toHaveBeenCalled();

		const [, init] = fetchMock.mock.calls[0] as [string, RequestInit];
		const h = init.headers as Record<string, string>;
		expect(h).not.toHaveProperty('Authorization');
	});

	it('calls unauthorized handler on 401 responses', async () => {
		const onUnauthorized = vi.fn();
		globalThis.fetch = vi.fn(async () => {
			return new Response(null, { status: 401 });
		}) as unknown as typeof fetch;

		const client = createApiClient({
			bearerToken: 'abc',
			onUnauthorized
		});

		await expect(client.get('/rest/protected')).rejects.toMatchObject({ status: 401 });
		expect(onUnauthorized).toHaveBeenCalled();
	});

	it('does not call unauthorized handler on 403 responses', async () => {
		const onUnauthorized = vi.fn();
		globalThis.fetch = vi.fn(async () => {
			return new Response(null, { status: 403 });
		}) as unknown as typeof fetch;

		const client = createApiClient({
			bearerToken: 'abc',
			onUnauthorized
		});

		await expect(client.get('/rest/forbidden')).rejects.toMatchObject({ status: 403 });
		expect(onUnauthorized).not.toHaveBeenCalled();
	});

	it('sends authenticated requests without client-side token expiry prechecks', async () => {
		const client = createApiClient({
			bearerToken: 'token-without-local-expiry'
		});

		await client.get('/rest/protected');

		expect(globalThis.fetch).toHaveBeenCalledOnce();
	});

	it('parses { error, message } payload: errorCode is set, message is preferred', async () => {
		globalThis.fetch = vi.fn(async () => {
			return new Response(
				JSON.stringify({ ok: false, error: 'auth/invalid_credentials', message: 'Bad password' }),
				{ status: 401, headers: { 'content-type': 'application/json' } }
			);
		}) as unknown as typeof fetch;

		const client = createApiClient({ bearerToken: '' });
		try {
			await client.get('/rest/test');
			expect.unreachable();
		} catch (err: unknown) {
			const apiErr = err as InstanceType<typeof import('./apiClient').ApiError>;
			expect(apiErr.status).toBe(401);
			expect(apiErr.errorCode).toBe('auth/invalid_credentials');
			expect(apiErr.message).toBe('Bad password');
			expect(apiErr.serverMessage).toBe('Bad password');
		}
	});

	it('parses { error } only payload: errorCode and message both use the code', async () => {
		globalThis.fetch = vi.fn(async () => {
			return new Response(JSON.stringify({ ok: false, error: 'fs/file_not_found' }), {
				status: 404,
				headers: { 'content-type': 'application/json' }
			});
		}) as unknown as typeof fetch;

		const client = createApiClient({ bearerToken: '' });
		try {
			await client.get('/rest/test');
			expect.unreachable();
		} catch (err: unknown) {
			const apiErr = err as InstanceType<typeof import('./apiClient').ApiError>;
			expect(apiErr.status).toBe(404);
			expect(apiErr.errorCode).toBe('fs/file_not_found');
			expect(apiErr.message).toBe('fs/file_not_found');
		}
	});

	it('handles non-JSON error response gracefully', async () => {
		globalThis.fetch = vi.fn(async () => {
			return new Response('Internal Server Error', {
				status: 500,
				headers: { 'content-type': 'text/plain' }
			});
		}) as unknown as typeof fetch;

		const client = createApiClient({ bearerToken: '' });
		try {
			await client.get('/rest/test');
			expect.unreachable();
		} catch (err: unknown) {
			const apiErr = err as InstanceType<typeof import('./apiClient').ApiError>;
			expect(apiErr.status).toBe(500);
			expect(apiErr.errorCode).toBeUndefined();
			expect(apiErr.message).toContain('GET');
		}
	});
});
