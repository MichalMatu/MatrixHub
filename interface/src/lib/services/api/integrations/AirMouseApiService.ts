import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { AirMouseStatus, AirMouseConfig } from '$lib/types/devices/airmouse';

export class AirMouseApiService {
	private client;
	private static readonly TIMEOUT_MS = 5000;
	private static readonly CALIBRATE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(signal?: AbortSignal): Promise<AirMouseStatus> {
		return this.client.get<AirMouseStatus>('/api/airmouse/status', {
			signal: signal
				? AbortSignal.any([signal, AbortSignal.timeout(AirMouseApiService.TIMEOUT_MS)])
				: AbortSignal.timeout(AirMouseApiService.TIMEOUT_MS)
		});
	}

	async updateConfig(config: AirMouseConfig): Promise<AirMouseConfig> {
		return this.client.post<AirMouseConfig>('/api/airmouse/config', config, {
			signal: AbortSignal.timeout(AirMouseApiService.TIMEOUT_MS)
		});
	}

	async calibrate(): Promise<{ status: string }> {
		return this.client.post<{ status: string }>(
			'/api/airmouse/calibrate',
			{},
			{
				signal: AbortSignal.timeout(AirMouseApiService.CALIBRATE_TIMEOUT_MS)
			}
		);
	}
}
