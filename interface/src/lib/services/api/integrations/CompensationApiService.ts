/**
 * @file CompensationApiService.ts
 * @brief API service for SCD41 temperature compensation tuning
 *
 * Controls dynamic temperature offset calculation parameters
 * that compensate for CPU-induced heating of the SCD41 sensor.
 */

import { createApiClient, type ApiClientOptions } from '$lib/utils';

export interface CompensationSettings {
	enabled: boolean;
	base_temp_offset: number;
	reference_cpu_temp: number;
	temp_offset_per_cpu_degree: number;
	min_temp_offset: number;
	max_temp_offset: number;
}

export class CompensationApiService {
	private client;
	static readonly GET_TIMEOUT_MS = 5000;
	static readonly UPDATE_TIMEOUT_MS = 8000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getSettings(): Promise<CompensationSettings> {
		return this.client.get<CompensationSettings>('/api/compensation', {
			signal: AbortSignal.timeout(CompensationApiService.GET_TIMEOUT_MS)
		});
	}

	async updateSettings(settings: CompensationSettings): Promise<CompensationSettings> {
		return this.client.post<CompensationSettings>('/api/compensation', settings, {
			signal: AbortSignal.timeout(CompensationApiService.UPDATE_TIMEOUT_MS)
		});
	}
}
