import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { WifiSensingSettings, WifiSensingStatus } from '$lib/types/connectivity/wifiSensing';
import type { WifiStatus } from '$lib/types/connectivity/wifi';

export class WifiSensingApiService {
	private client;
	private static readonly STATS_TIMEOUT_MS = 20000;
	private static readonly SETTINGS_TIMEOUT_MS = 20000;
	private static readonly SAVE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	// getBinaryData removed (legacy endpoint /api/wifisensing/binary removed)

	async getSettings(): Promise<WifiSensingSettings> {
		return this.client.get<WifiSensingSettings>('/api/wifisensing/config', {
			signal: AbortSignal.timeout(WifiSensingApiService.SETTINGS_TIMEOUT_MS)
		});
	}

	async getStatus(): Promise<WifiSensingStatus> {
		return this.client.get<WifiSensingStatus>('/api/wifisensing/status', {
			signal: AbortSignal.timeout(WifiSensingApiService.STATS_TIMEOUT_MS)
		});
	}

	async saveSettings(settings: Partial<WifiSensingSettings>): Promise<WifiSensingSettings> {
		return this.client.post<WifiSensingSettings>('/api/wifisensing/config', settings, {
			signal: AbortSignal.timeout(WifiSensingApiService.SAVE_TIMEOUT_MS)
		});
	}

	async calibrateCsiAlarm(): Promise<{ ok: boolean; state?: string; error?: string }> {
		return this.client.post(
			'/api/wifisensing/csi/calibrate',
			{},
			{
				signal: AbortSignal.timeout(WifiSensingApiService.SAVE_TIMEOUT_MS)
			}
		);
	}

	async getWifiStatus(): Promise<WifiStatus> {
		// Reusing the core WiFi status endpoint to get channel/SSID info
		return this.client.get<WifiStatus>('/rest/wifiStatus', {
			signal: AbortSignal.timeout(WifiSensingApiService.STATS_TIMEOUT_MS)
		});
	}
}
