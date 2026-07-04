import { createApiClient, type ApiClientOptions } from '$lib/utils';

export interface KeyboardTypePayload {
	text: string;
	enter: boolean;
}

export interface KeyboardConfig {
	enabled: boolean;
}

export type KeyboardPressPayload = { key: number } | { keys: number[] };

export class KeyboardApiService {
	private client;
	private static readonly CONFIG_TIMEOUT_MS = 5000;
	private static readonly TYPE_TIMEOUT_MS = 5000;
	private static readonly PRESS_TIMEOUT_MS = 5000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getConfig(): Promise<KeyboardConfig> {
		return await this.client.get('/api/keyboard/config', {
			signal: AbortSignal.timeout(KeyboardApiService.CONFIG_TIMEOUT_MS)
		});
	}

	async saveConfig(payload: KeyboardConfig): Promise<KeyboardConfig> {
		return await this.client.post('/api/keyboard/config', payload, {
			signal: AbortSignal.timeout(KeyboardApiService.CONFIG_TIMEOUT_MS)
		});
	}

	async typeText(payload: KeyboardTypePayload): Promise<void> {
		await this.client.postVoid('/api/keyboard/type', payload, {
			signal: AbortSignal.timeout(KeyboardApiService.TYPE_TIMEOUT_MS)
		});
	}

	async pressKey(payload: KeyboardPressPayload): Promise<void> {
		await this.client.postVoid('/api/keyboard/press', payload, {
			signal: AbortSignal.timeout(KeyboardApiService.PRESS_TIMEOUT_MS)
		});
	}
}
