import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { NTPSettings, NTPStatus } from '$lib/types/connectivity/ntp';

export class NtpApiService {
	private client;
	private static readonly STATUS_TIMEOUT_MS = 15000;
	private static readonly SETTINGS_TIMEOUT_MS = 15000;
	private static readonly SAVE_TIMEOUT_MS = 20000;
	private static readonly SET_TIME_TIMEOUT_MS = 15000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(): Promise<NTPStatus> {
		return this.client.get<NTPStatus>('/rest/ntpStatus', {
			signal: AbortSignal.timeout(NtpApiService.STATUS_TIMEOUT_MS)
		});
	}

	async getSettings(): Promise<NTPSettings> {
		return this.client.get<NTPSettings>('/rest/ntpSettings', {
			signal: AbortSignal.timeout(NtpApiService.SETTINGS_TIMEOUT_MS)
		});
	}

	async saveSettings(settings: NTPSettings): Promise<NTPSettings> {
		return this.client.post<NTPSettings>('/rest/ntpSettings', settings, {
			signal: AbortSignal.timeout(NtpApiService.SAVE_TIMEOUT_MS)
		});
	}

	async setTime(localTime: string): Promise<void> {
		await this.client.post(
			'/rest/time',
			{ local_time: localTime },
			{
				signal: AbortSignal.timeout(NtpApiService.SET_TIME_TIMEOUT_MS)
			}
		);
	}
}
