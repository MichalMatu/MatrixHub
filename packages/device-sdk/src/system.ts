import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type {
  AppConfig,
  SystemInformation,
  SystemNetworkInfo,
  TasksResponse,
  WifiRecoveryResponse,
} from "./types";

export class DeviceSystemApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSystemInfo(): Promise<SystemInformation> {
    return this.client.get<SystemInformation>("/api/system/info");
  }

  async getSystemTasks(options: { details?: boolean } = {}): Promise<TasksResponse> {
    const path = options.details ? "/api/system/tasks?details=1" : "/api/system/tasks";
    return this.client.get<TasksResponse>(path);
  }

  async getSystemNetwork(): Promise<SystemNetworkInfo> {
    return this.client.get<SystemNetworkInfo>("/api/system/network");
  }

  async getConfig(): Promise<AppConfig> {
    return this.client.get<AppConfig>("/api/config");
  }

  async saveConfig(config: AppConfig): Promise<AppConfig> {
    return this.client.post<AppConfig>("/api/config", config);
  }

  async triggerWifiRecovery(): Promise<WifiRecoveryResponse> {
    return this.client.post<WifiRecoveryResponse>("/api/system/wifi/recover");
  }
}
