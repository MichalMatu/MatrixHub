import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type { PowerConfig, PowerStatus } from "./types";

export class DevicePowerApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getStatus(): Promise<PowerStatus> {
    return this.client.get<PowerStatus>("/rest/power/status");
  }

  async getConfig(): Promise<PowerConfig> {
    return this.client.get<PowerConfig>("/rest/power/config");
  }

  async updateConfig(config: Partial<PowerConfig>): Promise<PowerConfig> {
    return this.client.post<PowerConfig>("/rest/power/config", config);
  }

  async restart(): Promise<void> {
    await this.client.postVoid("/rest/restart");
  }

  async factoryReset(): Promise<void> {
    await this.client.postVoid("/rest/factoryReset");
  }

  async requestSleep(): Promise<void> {
    await this.client.postVoid("/rest/sleep");
  }

  async requestHygieneSleep(): Promise<void> {
    await this.client.postVoid("/rest/power/hygieneSleep");
  }
}
