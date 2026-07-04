import { describe, it, expect, vi, beforeEach } from 'vitest';
import { AlarmsApiService } from './AlarmsApiService';

// Mock createApiClient
const mockClient = {
	get: vi.fn(),
	post: vi.fn(),
	fetch: vi.fn()
};

vi.mock('$lib/utils', () => ({
	createApiClient: () => mockClient
}));

describe('AlarmsApiService', () => {
	let service: AlarmsApiService;

	beforeEach(() => {
		vi.clearAllMocks();
		service = new AlarmsApiService({ bearerToken: 'token' });
	});

	it('should fetch rules without status', async () => {
		const mockResponse = { rules: [] };
		mockClient.get.mockResolvedValue(mockResponse);

		const result = await service.getRules();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/alarms/rules',
			expect.objectContaining({
				signal: expect.any(AbortSignal)
			})
		);
		expect(result).toBe(mockResponse);
	});

	it('should fetch rules with status', async () => {
		const mockResponse = { rules: [] };
		mockClient.get.mockResolvedValue(mockResponse);

		await service.getRules({ includeStatus: true });

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/alarms/rules?includeStatus=1',
			expect.any(Object)
		);
	});

	it('should save rules', async () => {
		const config = { schema_version: 1 as const, rules: [] };
		mockClient.post.mockResolvedValue(config);

		await service.saveRules(config);

		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/alarms/rules',
			config,
			expect.objectContaining({
				signal: expect.any(AbortSignal)
			})
		);
	});
});
