import { ApiError, createApiClient, type ApiClientOptions } from '$lib/utils';
import type { WifiSettings, WifiStatus, NetworkItem } from '$lib/types/connectivity/wifi';

export type NetworkScanState = 'idle' | 'running' | 'ready';

export interface NetworkListResponse {
	networks: NetworkItem[];
	scan_state?: NetworkScanState;
}

interface ErrorPayload {
	error?: string;
	message?: string;
}

async function toApiError(response: Response, fallbackMessage: string): Promise<ApiError> {
	let errorCode: string | undefined;
	let serverMessage: string | undefined;

	try {
		const contentType = response.headers.get('content-type');
		if (contentType?.includes('application/json')) {
			const body = (await response.json()) as ErrorPayload;
			if (typeof body.error === 'string') {
				errorCode = body.error;
			}
			if (typeof body.message === 'string') {
				serverMessage = body.message;
			}
		}
	} catch {
		// Fall back to the generic message when the error body is absent or malformed.
	}

	return new ApiError(response.status, fallbackMessage, serverMessage ?? errorCode, errorCode);
}

export class WifiApiService {
	private client;
	private static readonly STATUS_TIMEOUT_MS = 20000;
	private static readonly SETTINGS_TIMEOUT_MS = 20000;
	private static readonly SAVE_TIMEOUT_MS = 20000;

	private static readonly WIFI_SCAN_TIMEOUT_MS = 20000;
	private static readonly WIFI_LIST_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(): Promise<WifiStatus> {
		return this.client.get<WifiStatus>('/rest/wifiStatus', {
			signal: AbortSignal.timeout(WifiApiService.STATUS_TIMEOUT_MS)
		});
	}

	async getSettings(): Promise<WifiSettings> {
		return this.client.get<WifiSettings>('/rest/wifiSettings', {
			signal: AbortSignal.timeout(WifiApiService.SETTINGS_TIMEOUT_MS)
		});
	}

	async saveSettings(settings: WifiSettings): Promise<WifiSettings> {
		// Sanitize networks - remove undefined values
		const sanitizedNetworks = settings.wifi_networks.map((network) => {
			const copy: Record<string, unknown> = { ...network };
			Object.keys(copy).forEach((key) => {
				if (copy[key] === undefined) delete copy[key];
			});
			return copy;
		});

		const payload = {
			...settings,
			hostname: settings.hostname || 'matrixhub',
			wifi_networks: sanitizedNetworks
		};

		return this.client.post<WifiSettings>('/rest/wifiSettings', payload, {
			signal: AbortSignal.timeout(WifiApiService.SAVE_TIMEOUT_MS)
		});
	}

	async scanNetworks(): Promise<void> {
		const res = await this.client.fetch('/rest/scanNetworks', {
			method: 'GET',
			signal: AbortSignal.timeout(WifiApiService.WIFI_SCAN_TIMEOUT_MS)
		});
		if (!res.ok) {
			throw await toApiError(res, `GET /rest/scanNetworks failed`);
		}
	}

	async listNetworks(): Promise<NetworkListResponse> {
		return this.client.get<NetworkListResponse>('/rest/listNetworks', {
			signal: AbortSignal.timeout(WifiApiService.WIFI_LIST_TIMEOUT_MS)
		});
	}
}
