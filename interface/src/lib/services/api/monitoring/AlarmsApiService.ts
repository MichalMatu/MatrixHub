import { createApiClient, type ApiClientOptions } from '$lib/utils';
import type { AlarmRulesConfig } from '$lib/types/domain/alarms';

export class AlarmsApiService {
	private client;
	private static readonly GET_RULES_TIMEOUT_MS = 20000;
	private static readonly SAVE_RULES_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getRules(options?: { includeStatus?: boolean }): Promise<AlarmRulesConfig> {
		const includeStatus = options?.includeStatus === true;
		const url = includeStatus ? '/api/alarms/rules?includeStatus=1' : '/api/alarms/rules';
		return this.client.get<AlarmRulesConfig>(url, {
			signal: AbortSignal.timeout(AlarmsApiService.GET_RULES_TIMEOUT_MS)
		});
	}

	async saveRules(config: AlarmRulesConfig): Promise<AlarmRulesConfig> {
		return this.client.post<AlarmRulesConfig>('/api/alarms/rules', config, {
			signal: AbortSignal.timeout(AlarmsApiService.SAVE_RULES_TIMEOUT_MS)
		});
	}
}
