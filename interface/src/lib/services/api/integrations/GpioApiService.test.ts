import { beforeEach, describe, expect, it, vi } from 'vitest';
import { GpioApiService } from './GpioApiService';

const mockClient = {
	get: vi.fn(),
	post: vi.fn()
};

vi.mock('$lib/utils', () => ({
	createApiClient: () => mockClient
}));

describe('GpioApiService', () => {
	let service: GpioApiService;

	beforeEach(() => {
		vi.clearAllMocks();
		service = new GpioApiService({ bearerToken: 'token' });
	});

	it('fetches allowed pins', async () => {
		const pins = [{ id: 'gpio38', pin: 38 }];
		mockClient.get.mockResolvedValue(pins);

		await expect(service.getPins()).resolves.toBe(pins);
		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/gpio/pins',
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});

	it('saves GPIO config', async () => {
		const config = { channels: [] };
		mockClient.post.mockResolvedValue(config);

		await expect(service.saveConfig(config)).resolves.toBe(config);
		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/gpio/config',
			config,
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});

	it('updates output state', async () => {
		mockClient.post.mockResolvedValue({});

		await service.setOutput({ id: 'gpio38', value: true });

		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/gpio/output',
			{ id: 'gpio38', value: true },
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});
});
