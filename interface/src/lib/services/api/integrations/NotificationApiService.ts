import { createApiClient, type ApiClientOptions } from '$lib/utils';

export interface NotificationSettings {
	telegram_enabled: boolean;
	webhook_enabled: boolean;
	bot_token: string;
	chat_id: string;
	commands_enabled: boolean;
	webhook_url: string;
	pushover_enabled: boolean;
	pushover_user: string;
	pushover_token: string;
	is_configured: boolean;
}

export class NotificationApiService {
	private client;
	private static readonly GET_SETTINGS_TIMEOUT_MS = 15000;
	private static readonly UPDATE_SETTINGS_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getSettings(): Promise<NotificationSettings> {
		return this.client.get<NotificationSettings>('/api/notifications/settings', {
			signal: AbortSignal.timeout(NotificationApiService.GET_SETTINGS_TIMEOUT_MS)
		});
	}

	async updateSettings(settings: Partial<NotificationSettings>): Promise<NotificationSettings> {
		return this.client.post<NotificationSettings>('/api/notifications/settings', settings, {
			signal: AbortSignal.timeout(NotificationApiService.UPDATE_SETTINGS_TIMEOUT_MS)
		});
	}

	async testTelegram(
		message: string,
		signal?: AbortSignal
	): Promise<{
		ok: boolean;
		configured?: boolean;
		httpCode?: number;
		error?: string;
		response?: string;
	}> {
		const response = await this.client.fetch('/api/notifications/telegram/test', {
			method: 'POST',
			body: JSON.stringify({ text: message }),
			signal
		});

		let data;
		try {
			data = await response.json();
		} catch {
			// ignore
		}

		if (!data) throw new Error('Invalid response');
		return data;
	}

	async testWebhook(
		content: string,
		signal?: AbortSignal
	): Promise<{
		ok: boolean;
		configured?: boolean;
		httpCode?: number;
		error?: string;
		response?: string;
	}> {
		const response = await this.client.fetch('/api/notifications/webhook/test', {
			method: 'POST',
			body: JSON.stringify({ content }),
			signal
		});

		let data;
		try {
			data = await response.json();
		} catch {
			// ignore
		}

		if (!data) throw new Error('Invalid response');
		return data;
	}

	async testPushover(
		message: string,
		signal?: AbortSignal
	): Promise<{
		ok: boolean;
		configured?: boolean;
		httpCode?: number;
		error?: string;
	}> {
		const response = await this.client.fetch('/api/notifications/pushover/test', {
			method: 'POST',
			body: JSON.stringify({ message }),
			signal
		});

		let data;
		try {
			data = await response.json();
		} catch {
			// ignore
		}

		if (!data) throw new Error('Invalid response');
		return data;
	}
}
