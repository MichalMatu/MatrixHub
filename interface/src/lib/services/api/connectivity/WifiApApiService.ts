import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { ApSettings, ApStatus } from '$lib/types/connectivity/ap';

export class WifiApApiService {
	private client;
	private static readonly STATUS_TIMEOUT_MS = 15000;
	private static readonly SETTINGS_TIMEOUT_MS = 15000;
	private static readonly SAVE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(): Promise<ApStatus> {
		return this.client.get<ApStatus>('/rest/apStatus', {
			signal: AbortSignal.timeout(WifiApApiService.STATUS_TIMEOUT_MS)
		});
	}

	async getSettings(): Promise<ApSettings> {
		return this.client.get<ApSettings>('/rest/apSettings', {
			signal: AbortSignal.timeout(WifiApApiService.SETTINGS_TIMEOUT_MS)
		});
	}

	async saveSettings(settings: ApSettings): Promise<ApSettings> {
		return this.client.post<ApSettings>('/rest/apSettings', settings, {
			signal: AbortSignal.timeout(WifiApApiService.SAVE_TIMEOUT_MS)
		});
	}
}
