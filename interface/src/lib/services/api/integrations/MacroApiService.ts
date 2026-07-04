import { createApiClient, type ApiClientOptions } from '$lib/utils/api/apiClient';

export interface ScriptStatus {
	current_script: string;
	status: 'IDLE' | 'RUNNING' | 'PAUSED' | 'ERROR' | 'COMPLETED';
	current_line: number;
	uptime_ms: number;
	last_error: string;
}

export interface ScriptFile {
	name: string;
}

export interface MacroSettings {
	enabled: boolean;
	boot_script: string;
	boot_delay: number;
}

export interface MacroActionResponse {
	ok: boolean;
	status?: 'saved' | 'deleted' | 'started' | 'stopped' | 'no_changes';
	error?: string;
}

export class MacroApiService {
	private readonly baseUrl = '/api/macros';
	private readonly client;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async listScripts(signal?: AbortSignal): Promise<ScriptFile[]> {
		return this.client.get<ScriptFile[]>(this.baseUrl, { signal });
	}

	async uploadScript(filename: string, content: string): Promise<MacroActionResponse> {
		return this.client.post<MacroActionResponse>(this.baseUrl, { filename, content });
	}

	async deleteScript(filename: string): Promise<MacroActionResponse> {
		return this.client.post<MacroActionResponse>(`${this.baseUrl}/delete`, { name: filename });
	}

	async runScript(filename: string): Promise<MacroActionResponse> {
		return this.client.post<MacroActionResponse>(`${this.baseUrl}/run`, { name: filename });
	}

	async stopScript(): Promise<MacroActionResponse> {
		return this.client.post<MacroActionResponse>(`${this.baseUrl}/stop`);
	}

	async getStatus(signal?: AbortSignal): Promise<ScriptStatus> {
		return this.client.get<ScriptStatus>(`${this.baseUrl}/status`, { signal });
	}

	async getScriptContent(filename: string, signal?: AbortSignal): Promise<string> {
		const safeName = encodeURIComponent(filename);
		const response = await this.client.fetch(`${this.baseUrl}/content?name=${safeName}`, {
			signal
		});
		if (!response.ok) throw new Error('Failed to get content');
		return response.text();
	}

	async getSettings(signal?: AbortSignal): Promise<MacroSettings> {
		return this.client.get<MacroSettings>(`${this.baseUrl}/settings`, { signal });
	}

	async saveSettings(settings: MacroSettings): Promise<MacroSettings> {
		return this.client.post<MacroSettings>(`${this.baseUrl}/settings`, settings);
	}
}
