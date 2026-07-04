import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type { AlarmRulesConfig } from "./types";

export class DeviceAlarmsApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getRules(options: { includeStatus?: boolean } = {}): Promise<AlarmRulesConfig> {
    const path = options.includeStatus ? "/api/alarms/rules?includeStatus=1" : "/api/alarms/rules";
    return this.client.get<AlarmRulesConfig>(path);
  }

  async saveRules(config: AlarmRulesConfig): Promise<AlarmRulesConfig> {
    return this.client.post<AlarmRulesConfig>("/api/alarms/rules", config);
  }
}
