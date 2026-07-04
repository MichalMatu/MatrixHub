import { describe, it, expect, vi, beforeEach } from 'vitest';
import { WifiApiService } from './WifiApiService';

// Mock createApiClient
const mockClient = {
	get: vi.fn(),
	post: vi.fn(),
	fetch: vi.fn()
};

vi.mock('$lib/utils', () => ({
	createApiClient: () => mockClient,
	ApiError: class ApiError extends Error {
		serverMessage?: string;
		errorCode?: string;

		constructor(
			public status: number,
			message: string,
			serverMessage?: string,
			errorCode?: string
		) {
			super(serverMessage || message);
			this.name = 'ApiError';
			this.serverMessage = serverMessage;
			this.errorCode = errorCode;
		}
	}
}));

describe('WifiApiService', () => {
	let service: WifiApiService;

	beforeEach(() => {
		vi.clearAllMocks();
		service = new WifiApiService({ bearerToken: 'token' });
	});

	it('should get status', async () => {
		const mockStatus = { connected: true, ip: '1.2.3.4' };
		mockClient.get.mockResolvedValue(mockStatus);

		const result = await service.getStatus();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/rest/wifiStatus',
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
		expect(result).toBe(mockStatus);
	});

	it('should get settings', async () => {
		const mockSettings = { wifi_networks: [] };
		mockClient.get.mockResolvedValue(mockSettings);

		await service.getSettings();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/rest/wifiSettings',
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});

	it('should save settings, sanitize networks, and apply the matrixhub hostname fallback', async () => {
		const settings = {
			hostname: '',
			mode: 'ap',
			wifi_networks: [
				{ ssid: 'Net1', pass: 'pass1', bssid: undefined },
				{ ssid: 'Net2', pass: 'pass2' }
			]
			// eslint-disable-next-line @typescript-eslint/no-explicit-any
		} as any;

		mockClient.post.mockResolvedValue(settings);

		await service.saveSettings(settings);

		expect(mockClient.post).toHaveBeenCalledWith(
			'/rest/wifiSettings',
			expect.objectContaining({
				hostname: 'matrixhub',
				mode: 'ap',
				wifi_networks: [
					// undefined 'bssid' should be removed
					{ ssid: 'Net1', pass: 'pass1' },
					{ ssid: 'Net2', pass: 'pass2' }
				]
			}),
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});

	it('should scan networks', async () => {
		mockClient.fetch.mockResolvedValue({ ok: true });

		await service.scanNetworks();

		expect(mockClient.fetch).toHaveBeenCalledWith(
			'/rest/scanNetworks',
			expect.objectContaining({
				method: 'GET',
				signal: expect.any(AbortSignal)
			})
		);
	});

	it('should preserve backend error details on scan failure', async () => {
		mockClient.fetch.mockResolvedValue(
			new Response(JSON.stringify({ error: 'wifi/scan_failed', message: 'Scan failed' }), {
				status: 503,
				headers: { 'content-type': 'application/json' }
			})
		);

		await expect(service.scanNetworks()).rejects.toMatchObject({
			status: 503,
			errorCode: 'wifi/scan_failed',
			message: 'Scan failed'
		});
	});

	it('should list networks', async () => {
		const mockList = { scan_state: 'running', networks: [] };
		mockClient.get.mockResolvedValue(mockList);

		const result = await service.listNetworks();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/rest/listNetworks',
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
		expect(result).toBe(mockList);
	});
});
