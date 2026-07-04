/**
 * @file HeartbeatApiService.ts
 * @brief API service for multi-slot heartbeat pinger settings
 *
 * Supports Healthchecks.io, Uptime Kuma Push, Cronitor, Better Uptime, etc.
 */

import { createApiClient, type ApiClientOptions } from '$lib/utils';

export interface HeartbeatSlot {
	enabled: boolean;
	name: string;
	url: string;
	allow_insecure: boolean;
}

export interface HeartbeatSettings {
	interval_ms: number;
	slots: HeartbeatSlot[];
}

type HeartbeatTestStatus = 'queued' | 'no_enabled_slots' | 'ping_failed';

export interface HeartbeatTestResult {
	success: boolean;
	message: string;
	status?: HeartbeatTestStatus;
	active_slots?: number;
	retry_count?: number;
	timeout_ms?: number;
	retry_after_ms?: number;
}

export class HeartbeatApiService {
	private client;
	public static readonly GET_TIMEOUT_MS = 15000;
	public static readonly UPDATE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getSettings(): Promise<HeartbeatSettings> {
		return this.client.get<HeartbeatSettings>('/api/heartbeat', {
			signal: AbortSignal.timeout(HeartbeatApiService.GET_TIMEOUT_MS)
		});
	}

	async updateSettings(settings: HeartbeatSettings): Promise<HeartbeatSettings> {
		return this.client.post<HeartbeatSettings>('/api/heartbeat', settings, {
			signal: AbortSignal.timeout(HeartbeatApiService.UPDATE_TIMEOUT_MS)
		});
	}

	async testPing(): Promise<HeartbeatTestResult> {
		return this.client.post<HeartbeatTestResult>('/api/heartbeat/test', undefined, {
			signal: AbortSignal.timeout(10000)
		});
	}
}
