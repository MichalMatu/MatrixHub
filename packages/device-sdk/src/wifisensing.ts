import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type { WifiSensingSettings, WifiSensingStatus, WifiStatus } from "./types";
import { buildDeviceWebSocketUrl } from "./ws";

export const CSI_WEBSOCKET_PATH = "/ws/csi";

export function buildCsiWebSocketUrl(origin: string) {
  return buildDeviceWebSocketUrl(origin, CSI_WEBSOCKET_PATH);
}

export class DeviceWifiSensingApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSettings(): Promise<WifiSensingSettings> {
    return this.client.get<WifiSensingSettings>("/api/wifisensing/config");
  }

  async saveSettings(settings: Partial<WifiSensingSettings>): Promise<WifiSensingSettings> {
    return this.client.post<WifiSensingSettings>("/api/wifisensing/config", settings);
  }

  async getStatus(): Promise<WifiSensingStatus> {
    return this.client.get<WifiSensingStatus>("/api/wifisensing/status");
  }

  async getWifiStatus(): Promise<WifiStatus> {
    return this.client.get<WifiStatus>("/rest/wifiStatus");
  }
}
