import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { SystemInformation } from '$lib/types/system/system';
import { SystemInformationSchema, WifiRecoveryResponseSchema } from '$lib/utils/validation/schemas';

export type { SystemInformation };
export interface WifiRecoveryResponse {
	success: boolean;
	accepted: boolean;
	connected: boolean;
	ip?: string;
	rssi?: number;
}

interface TailLine {
	t: number; // timestampMs
	l: string; // level
	g: string; // tag
	m: string; // message
}

export interface TailResponse {
	capacity?: number;
	lines: TailLine[];
}

interface TailClearResponse {
	ok: boolean;
	status: string;
}

export interface LoggingConfig {
	level: string;
}

export interface AppConfig {
	logging?: LoggingConfig;
	[key: string]: unknown;
}

export class SystemApiService {
	private client;
	private static readonly SYSTEM_STATUS_TIMEOUT_MS = 15000;
	private static readonly GET_CONFIG_TIMEOUT_MS = 15000;
	private static readonly SAVE_CONFIG_TIMEOUT_MS = 20000;
	private static readonly TASKS_TIMEOUT_MS = 15000;
	private static readonly LOG_TAIL_TIMEOUT_MS = 15000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getSystemStatus(): Promise<SystemInformation> {
		return this.client.get<SystemInformation>('/api/system/info', {
			signal: AbortSignal.timeout(SystemApiService.SYSTEM_STATUS_TIMEOUT_MS),
			schema: SystemInformationSchema
		});
	}

	async triggerWifiRecovery(): Promise<WifiRecoveryResponse> {
		return this.client.post<WifiRecoveryResponse>('/api/system/wifi/recover', undefined, {
			schema: WifiRecoveryResponseSchema
		});
	}

	async getConfig(): Promise<AppConfig> {
		return this.client.get<AppConfig>('/api/config', {
			signal: AbortSignal.timeout(SystemApiService.GET_CONFIG_TIMEOUT_MS)
		});
	}

	async saveConfig(config: AppConfig): Promise<void> {
		await this.client.post('/api/config', config, {
			signal: AbortSignal.timeout(SystemApiService.SAVE_CONFIG_TIMEOUT_MS)
		});
	}

	async getLogTail(lines: number = 20, since?: number): Promise<TailResponse> {
		let url = `/rest/logs/tail?lines=${lines}`;
		if (since !== undefined) {
			url += `&since=${since}`;
		}
		return this.client.get<TailResponse>(url, {
			signal: AbortSignal.timeout(SystemApiService.LOG_TAIL_TIMEOUT_MS)
		});
	}

	async clearLogTail(): Promise<void> {
		await this.client.delete<TailClearResponse>('/rest/logs/tail', {
			signal: AbortSignal.timeout(SystemApiService.LOG_TAIL_TIMEOUT_MS)
		});
	}

	async getTasks(
		options: { signal?: AbortSignal; details?: boolean } = {}
	): Promise<TasksResponse> {
		const { signal, details = false } = options;
		const url = details ? '/api/system/tasks?details=1' : '/api/system/tasks';
		return this.client.get<TasksResponse>(url, {
			signal: signal
				? AbortSignal.any([signal, AbortSignal.timeout(SystemApiService.TASKS_TIMEOUT_MS)])
				: AbortSignal.timeout(SystemApiService.TASKS_TIMEOUT_MS)
		});
	}
}

export interface TaskInfo {
	name: string;
	priority: number;
	stackHighWaterMark: number;
	state: string;
	coreId?: number;
}

export interface TasksResponse {
	watchdog: {
		initialized: boolean;
		timeoutSec: number;
	};
	taskCount: number;
	detailsIncluded?: boolean;
	tasks?: TaskInfo[];
	error?: string;
	memory: {
		freeHeap: number;
		minFreeHeap: number;
		freePsram?: number;
	};
}
