import { describe, expect, it, vi } from 'vitest';
import { LogsApiService } from './LogsApiService';

function createService(fetchImpl: typeof fetch) {
	return new LogsApiService({
		bearerToken: 'token',
		fetch: fetchImpl
	});
}

describe('LogsApiService', () => {
	it('preserves backend error details for historical chart download failures', async () => {
		const fetchMock = vi.fn(async () => {
			return new Response(
				JSON.stringify({ ok: false, error: 'fs/busy', message: 'Storage busy' }),
				{
					status: 503,
					headers: { 'content-type': 'application/json' }
				}
			);
		}) as unknown as typeof fetch;

		const service = createService(fetchMock);

		await expect(service.getHistoricalChartData('2026-03-15')).rejects.toMatchObject({
			status: 503,
			errorCode: 'fs/busy',
			message: 'Storage busy'
		});
	});

	it('returns a blob for successful protected downloads', async () => {
		const fetchMock = vi.fn(async () => {
			return new Response('demo-binary', {
				status: 200,
				headers: { 'content-type': 'application/octet-stream' }
			});
		}) as unknown as typeof fetch;

		const service = createService(fetchMock);
		const blob = await service.downloadLog('/data/2026-03/day.bin');

		expect(fetchMock).toHaveBeenCalledWith(
			'/api/logs/download?file=%2Fdata%2F2026-03%2Fday.bin',
			expect.objectContaining({
				headers: expect.objectContaining({
					Authorization: 'Bearer token'
				})
			})
		);
		expect(await blob.text()).toBe('demo-binary');
	});

	it('throws an ApiError when protected download fails', async () => {
		const fetchMock = vi.fn(async () => {
			return new Response(JSON.stringify({ ok: false, error: 'fs/busy' }), {
				status: 503,
				headers: { 'content-type': 'application/json' }
			});
		}) as unknown as typeof fetch;

		const service = createService(fetchMock);

		await expect(service.downloadLog('/data/2026-03/day.bin')).rejects.toMatchObject({
			status: 503,
			errorCode: 'fs/busy',
			message: 'fs/busy'
		});
	});

	it('throws an ApiError when delete fails', async () => {
		const fetchMock = vi.fn(async () => {
			return new Response(JSON.stringify({ ok: false, error: 'not_found' }), {
				status: 404,
				headers: { 'content-type': 'application/json' }
			});
		}) as unknown as typeof fetch;

		const service = createService(fetchMock);

		await expect(service.deleteLog('/data/2026-03/day.bin')).rejects.toMatchObject({
			status: 404,
			errorCode: 'not_found',
			message: 'not_found'
		});
	});
});
