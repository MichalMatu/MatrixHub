/**
 * @file UdpApiService.ts
 * @brief API service for UDP data pusher settings
 *
 * Sends sensor data via UDP to LAN servers (InfluxDB, Telegraf, custom).
 */

import { createApiClient, type ApiClientOptions } from '$lib/utils';

export type UdpFormat = 'line' | 'json' | 'csv';

export interface UdpSettings {
	enabled: boolean;
	host: string;
	port: number;
	format: UdpFormat;
	interval_ms: number;
}

type UdpTestStatus =
	| 'queued'
	| 'sent'
	| 'not_configured'
	| 'worker_stopping'
	| 'wifi_disconnected'
	| 'send_failed'
	| 'unavailable';

export interface UdpTestResult {
	success: boolean;
	message: string;
	status?: UdpTestStatus;
	retry_after_ms?: number;
}

export class UdpApiService {
	private client;
	static readonly GET_TIMEOUT_MS = 5000;
	static readonly UPDATE_TIMEOUT_MS = 8000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getSettings(): Promise<UdpSettings> {
		return this.client.get<UdpSettings>('/api/udp', {
			signal: AbortSignal.timeout(UdpApiService.GET_TIMEOUT_MS)
		});
	}

	async updateSettings(settings: UdpSettings): Promise<UdpSettings> {
		return this.client.post<UdpSettings>('/api/udp', settings, {
			signal: AbortSignal.timeout(UdpApiService.UPDATE_TIMEOUT_MS)
		});
	}

	async testSend(): Promise<UdpTestResult> {
		return this.client.post<UdpTestResult>('/api/udp/test', undefined, {
			signal: AbortSignal.timeout(10000)
		});
	}
}
