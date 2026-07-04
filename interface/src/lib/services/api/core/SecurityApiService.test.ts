import { afterEach, describe, expect, it, vi } from 'vitest';
import { ApiError } from '$lib/utils';
import { signIn } from './SecurityApiService';

describe('signIn', () => {
	afterEach(() => {
		vi.unstubAllGlobals();
		vi.useRealTimers();
	});

	it('returns null for invalid credentials', async () => {
		vi.stubGlobal(
			'fetch',
			vi.fn(async () => {
				return new Response(JSON.stringify({ error: 'auth/invalid_credentials' }), {
					status: 401,
					headers: { 'content-type': 'application/json' }
				});
			}) as typeof fetch
		);

		await expect(signIn({ username: 'admin', password: 'wrong' })).resolves.toBeNull();
	});

	it('rethrows non-authentication failures', async () => {
		vi.stubGlobal(
			'fetch',
			vi.fn(async () => {
				return new Response(JSON.stringify({ error: 'server/unavailable' }), {
					status: 503,
					headers: { 'content-type': 'application/json' }
				});
			}) as typeof fetch
		);

		await expect(signIn({ username: 'admin', password: 'secret' })).rejects.toBeInstanceOf(
			ApiError
		);
	});

	it('rethrows forbidden responses instead of treating them as invalid credentials', async () => {
		vi.stubGlobal(
			'fetch',
			vi.fn(async () => {
				return new Response(JSON.stringify({ error: 'auth/forbidden' }), {
					status: 403,
					headers: { 'content-type': 'application/json' }
				});
			}) as typeof fetch
		);

		await expect(signIn({ username: 'admin', password: 'secret' })).rejects.toMatchObject({
			status: 403,
			errorCode: 'auth/forbidden'
		});
	});

	it('retries a timeout once before succeeding', async () => {
		vi.useFakeTimers();
		const fetchMock = vi
			.fn()
			.mockRejectedValueOnce(new DOMException('signal timed out', 'TimeoutError'))
			.mockResolvedValueOnce(
				new Response(JSON.stringify({ access_token: 'token-123' }), {
					status: 200,
					headers: { 'content-type': 'application/json' }
				})
			);
		vi.stubGlobal('fetch', fetchMock as typeof fetch);

		const resultPromise = signIn({ username: 'admin', password: 'secret' });
		await vi.runAllTimersAsync();

		await expect(resultPromise).resolves.toEqual({ access_token: 'token-123' });
		expect(fetchMock).toHaveBeenCalledTimes(2);
	});

	it('does not retry invalid credentials', async () => {
		const fetchMock = vi.fn(async () => {
			return new Response(JSON.stringify({ error: 'auth/invalid_credentials' }), {
				status: 401,
				headers: { 'content-type': 'application/json' }
			});
		});
		vi.stubGlobal('fetch', fetchMock as typeof fetch);

		await expect(signIn({ username: 'admin', password: 'wrong' })).resolves.toBeNull();
		expect(fetchMock).toHaveBeenCalledTimes(1);
	});
});
