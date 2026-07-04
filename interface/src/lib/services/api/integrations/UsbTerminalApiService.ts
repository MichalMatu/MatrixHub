import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { UsbTerminalData } from '$lib/types/connectivity/usbTerminal';

export class UsbTerminalApiService {
	private client;
	private static readonly TIMEOUT_MS = 5000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getConfig(): Promise<UsbTerminalData> {
		return this.client.get<UsbTerminalData>('/api/usbterminal/config', {
			signal: AbortSignal.timeout(UsbTerminalApiService.TIMEOUT_MS)
		});
	}

	async updateConfig(config: UsbTerminalData): Promise<UsbTerminalData> {
		return this.client.post<UsbTerminalData>('/api/usbterminal/config', config, {
			signal: AbortSignal.timeout(UsbTerminalApiService.TIMEOUT_MS)
		});
	}
}
