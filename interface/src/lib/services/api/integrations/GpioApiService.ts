import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type {
	GpioConfig,
	GpioOutputRequest,
	GpioPinDefinition,
	GpioStatus
} from '$lib/types/domain/gpio';

export class GpioApiService {
	private client;
	private static readonly READ_TIMEOUT_MS = 15000;
	private static readonly WRITE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getPins(): Promise<GpioPinDefinition[]> {
		return this.client.get<GpioPinDefinition[]>('/api/gpio/pins', {
			signal: AbortSignal.timeout(GpioApiService.READ_TIMEOUT_MS)
		});
	}

	async getConfig(): Promise<GpioConfig> {
		return this.client.get<GpioConfig>('/api/gpio/config', {
			signal: AbortSignal.timeout(GpioApiService.READ_TIMEOUT_MS)
		});
	}

	async saveConfig(config: GpioConfig): Promise<GpioConfig> {
		return this.client.post<GpioConfig>('/api/gpio/config', config, {
			signal: AbortSignal.timeout(GpioApiService.WRITE_TIMEOUT_MS)
		});
	}

	async getStatus(): Promise<GpioStatus> {
		return this.client.get<GpioStatus>('/api/gpio/status', {
			signal: AbortSignal.timeout(GpioApiService.READ_TIMEOUT_MS)
		});
	}

	async setOutput(request: GpioOutputRequest): Promise<void> {
		await this.client.post('/api/gpio/output', request, {
			signal: AbortSignal.timeout(GpioApiService.WRITE_TIMEOUT_MS)
		});
	}
}
