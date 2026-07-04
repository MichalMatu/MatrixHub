import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { PowerConfig, PowerStatus } from '$lib/types/system/power';

export class PowerApiService {
	private client;
	private static readonly STATUS_TIMEOUT_MS = 15000;
	private static readonly UPDATE_CONFIG_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(options: { timeoutMs?: number } = {}): Promise<PowerStatus> {
		const { timeoutMs = PowerApiService.STATUS_TIMEOUT_MS } = options;
		return this.client.get<PowerStatus>('/rest/power/status', {
			signal: AbortSignal.timeout(timeoutMs)
		});
	}

	async restart(): Promise<void> {
		return this.client.postVoid('/rest/restart');
	}

	async factoryReset(): Promise<void> {
		return this.client.postVoid('/rest/factoryReset');
	}

	async requestSleep(): Promise<void> {
		return this.client.postVoid('/rest/sleep');
	}

	async requestHygieneSleep(): Promise<void> {
		return this.client.postVoid('/rest/power/hygieneSleep');
	}

	async requestHygieneSleepWithTimeout(timeoutMs = 2000): Promise<void> {
		try {
			// Race network request with timeout to ensure UI updates even if connection hangs
			const timeoutPromise = new Promise<void>((_, reject) =>
				setTimeout(() => reject(new Error('Hygiene sleep request timeout')), timeoutMs)
			);
			await Promise.race([this.requestHygieneSleep(), timeoutPromise]);
		} catch (error) {
			// Ignore network errors/timeouts as device rebooting is the expected outcome
			console.warn('Hygiene sleep request ended (expected):', error);
		}
	}

	async updateConfig(config: Partial<PowerConfig>): Promise<PowerConfig> {
		return this.client.post<PowerConfig>('/rest/power/config', config, {
			signal: AbortSignal.timeout(PowerApiService.UPDATE_CONFIG_TIMEOUT_MS)
		});
	}
}
