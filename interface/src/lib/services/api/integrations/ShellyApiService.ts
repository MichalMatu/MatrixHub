import { createApiClient, type ApiClientOptions } from '$lib/utils';

export interface ShellyDevice {
	/** Unique ID - Firmware limit: kMaxShellyId=32 (max 31 chars) */
	id: string;
	/** Display name - Firmware limit: kMaxShellyName=64 (max 63 chars) */
	name: string;
	/** IP Address - Firmware limit: kMaxShellyIp=16 (max 15 chars) */
	ip: string;
	relay_index: number;
	generation?: number;
	enabled?: boolean;
	isOn?: boolean;
	isOnline?: boolean;
	power?: number;
	energy?: number;
	voltage?: number;
	current?: number;
	temperature?: number;
	rssi?: number;
	lastUpdate?: number;
}

export interface ShellyControlRequest {
	id: string;
	on: boolean;
}

export class ShellyApiService {
	private client;
	private static readonly WRITE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getDevices(): Promise<ShellyDevice[]> {
		return this.client.get<ShellyDevice[]>('/api/shelly/devices', {
			signal: AbortSignal.timeout(ShellyApiService.WRITE_TIMEOUT_MS)
		});
	}

	async addDevice(device: ShellyDevice): Promise<void> {
		await this.client.post('/api/shelly/devices', device, {
			signal: AbortSignal.timeout(ShellyApiService.WRITE_TIMEOUT_MS)
		});
	}

	async deleteDevice(id: string): Promise<void> {
		await this.client.delete(`/api/shelly/devices?id=${id}`, {
			signal: AbortSignal.timeout(ShellyApiService.WRITE_TIMEOUT_MS)
		});
	}

	async controlDevice(request: ShellyControlRequest): Promise<void> {
		await this.client.post('/api/shelly/control', request, {
			signal: AbortSignal.timeout(ShellyApiService.WRITE_TIMEOUT_MS)
		});
	}
}
