import { describe, expect, it, vi } from 'vitest';
import { DEFAULT_APP_FEATURES, fetchAppFeatures, resolveAppFeatures } from './appFeatures';

describe('resolveAppFeatures', () => {
	it('falls back to conservative defaults on invalid payload', () => {
		expect(resolveAppFeatures({ sleep: 'yes' })).toEqual(DEFAULT_APP_FEATURES);
	});

	it('merges valid payload with defaults', () => {
		expect(resolveAppFeatures({ ntp: false, firmware_version: '2.0.0' })).toEqual({
			...DEFAULT_APP_FEATURES,
			ntp: false,
			firmware_version: '2.0.0'
		});
	});
});

describe('fetchAppFeatures', () => {
	it('returns defaults when response is not ok', async () => {
		const fetchMock = vi
			.fn<typeof fetch>()
			.mockResolvedValue(new Response('fail', { status: 503, statusText: 'Service Unavailable' }));

		await expect(fetchAppFeatures(fetchMock)).resolves.toEqual(DEFAULT_APP_FEATURES);
	});

	it('returns defaults when response body is invalid json', async () => {
		const fetchMock = vi.fn<typeof fetch>().mockResolvedValue(
			new Response('not-json', {
				status: 200,
				headers: { 'Content-Type': 'application/json' }
			})
		);

		await expect(fetchAppFeatures(fetchMock)).resolves.toEqual(DEFAULT_APP_FEATURES);
	});
});
