import { createApiClient, ApiError, type ApiClientOptions } from '$lib/utils';
import { BleStatusSchema, BleSettingsSchema } from '$lib/utils/validation/schemas';
import type { BleStatus, BleSettings } from '$lib/types/connectivity/ble';

export class BleApiService {
	private client;
	private static readonly STATUS_TIMEOUT_MS = 20000;
	private static readonly SETTINGS_TIMEOUT_MS = 20000;
	private static readonly SAVE_TIMEOUT_MS = 20000;
	private static readonly SCAN_TIMEOUT_MS = 15000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(): Promise<BleStatus> {
		return this.client.get<BleStatus>('/api/ble/status', {
			signal: AbortSignal.timeout(BleApiService.STATUS_TIMEOUT_MS),
			schema: BleStatusSchema
		});
	}

	async getSettings(): Promise<BleSettings> {
		return this.client.get<BleSettings>('/api/ble/settings', {
			signal: AbortSignal.timeout(BleApiService.SETTINGS_TIMEOUT_MS),
			schema: BleSettingsSchema
		});
	}

	async saveSettings(settings: Partial<BleSettings>): Promise<BleSettings> {
		return this.client.post<BleSettings>('/api/ble/settings', settings, {
			signal: AbortSignal.timeout(BleApiService.SAVE_TIMEOUT_MS)
		});
	}

	async startScan(timeoutMs: number = 30000): Promise<void> {
		const response = await this.client.fetch(`/api/ble/scan?timeout=${timeoutMs}`, {
			method: 'POST',
			body: JSON.stringify({}),
			signal: AbortSignal.timeout(Math.min(15000, Math.max(1000, timeoutMs)))
		});

		if (!response.ok) {
			let errorMessage = `Failed to start scan: ${response.status}`;
			let errorCode: string | undefined;
			try {
				const body = await response.json();
				if (body && typeof body.error === 'string') {
					errorCode = body.error;
					errorMessage = typeof body.message === 'string' ? body.message : body.error;
				}
			} catch {
				// Response was not JSON, use default error
			}
			throw new ApiError(response.status, errorMessage, errorMessage, errorCode);
		}
	}

	async stopScan(): Promise<void> {
		const response = await this.client.fetch('/api/ble/scan', {
			method: 'DELETE',
			signal: AbortSignal.timeout(BleApiService.SCAN_TIMEOUT_MS)
		});

		if (!response.ok) {
			let errorMessage = `Failed to stop scan: ${response.status}`;
			let errorCode: string | undefined;
			try {
				const body = await response.json();
				if (body && typeof body.error === 'string') {
					errorCode = body.error;
					errorMessage = typeof body.message === 'string' ? body.message : body.error;
				}
			} catch {
				// Response was not JSON, use default error
			}
			throw new ApiError(response.status, errorMessage, errorMessage, errorCode);
		}
	}
}
