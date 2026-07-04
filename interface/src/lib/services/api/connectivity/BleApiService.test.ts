import { describe, it, expect, vi, beforeEach } from 'vitest';
import { BleApiService } from './BleApiService';
import { BleStatusSchema, BleSettingsSchema } from '$lib/utils/validation/schemas';

// Mock validation schemas
vi.mock('$lib/utils/validation/schemas', () => ({
	BleStatusSchema: {},
	BleSettingsSchema: {}
}));

// Mock createApiClient
const mockClient = {
	get: vi.fn(),
	post: vi.fn(),
	fetch: vi.fn()
};

vi.mock('$lib/utils', () => ({
	createApiClient: () => mockClient,
	ApiError: class ApiError extends Error {
		constructor(
			public status: number,
			message: string,
			public serverMessage?: string,
			public errorCode?: string
		) {
			super(serverMessage || message);
			this.name = 'ApiError';
		}
	}
}));

describe('BleApiService', () => {
	let service: BleApiService;

	beforeEach(() => {
		vi.clearAllMocks();
		service = new BleApiService({ bearerToken: 'token' });
	});

	it('should get status', async () => {
		const mockStatus = { running: true };
		mockClient.get.mockResolvedValue(mockStatus);

		const result = await service.getStatus();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/ble/status',
			expect.objectContaining({
				signal: expect.any(AbortSignal),
				schema: BleStatusSchema
			})
		);
		expect(result).toBe(mockStatus);
	});

	it('should get settings', async () => {
		const mockSettings = { sensors: [] };
		mockClient.get.mockResolvedValue(mockSettings);

		await service.getSettings();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/ble/settings',
			expect.objectContaining({ schema: BleSettingsSchema })
		);
	});

	it('should save settings', async () => {
		const settings = { sensors: [] };
		mockClient.post.mockResolvedValue(settings);

		await service.saveSettings(settings);

		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/ble/settings',
			settings,
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});

	it('should start scan with defaults', async () => {
		mockClient.fetch.mockResolvedValue({ ok: true });

		await service.startScan();

		expect(mockClient.fetch).toHaveBeenCalledWith(
			'/api/ble/scan?timeout=30000',
			expect.objectContaining({
				method: 'POST',
				body: '{}'
			})
		);
	});

	it('should start scan with custom timeout', async () => {
		mockClient.fetch.mockResolvedValue({ ok: true });

		await service.startScan(10000);

		expect(mockClient.fetch).toHaveBeenCalledWith(
			'/api/ble/scan?timeout=10000',
			expect.any(Object)
		);
	});

	it('should handle scan failure with remote error', async () => {
		mockClient.fetch.mockResolvedValue({
			ok: false,
			status: 400,
			json: async () => ({ error: 'Busy' })
		});

		await expect(service.startScan()).rejects.toThrow('Busy');
	});

	it('should handle scan failure with default error', async () => {
		mockClient.fetch.mockResolvedValue({
			ok: false,
			status: 500,
			json: async () => {
				throw new Error('Parse error');
			} // Body not JSON
		});

		await expect(service.startScan()).rejects.toThrow('Failed to start scan: 500');
	});
});
