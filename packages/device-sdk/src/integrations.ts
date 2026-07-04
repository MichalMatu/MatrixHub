import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type {
  CompensationSettings,
  HeartbeatSettings,
  HeartbeatTestResult,
  UdpSettings,
  UdpTestResult
} from "./types";

export class DeviceHeartbeatApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSettings(): Promise<HeartbeatSettings> {
    return this.client.get<HeartbeatSettings>("/api/heartbeat");
  }

  async updateSettings(settings: HeartbeatSettings): Promise<HeartbeatSettings> {
    return this.client.post<HeartbeatSettings>("/api/heartbeat", settings);
  }

  async testPing(): Promise<HeartbeatTestResult> {
    return this.client.post<HeartbeatTestResult>("/api/heartbeat/test");
  }
}

export class DeviceUdpApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSettings(): Promise<UdpSettings> {
    return this.client.get<UdpSettings>("/api/udp");
  }

  async updateSettings(settings: UdpSettings): Promise<UdpSettings> {
    return this.client.post<UdpSettings>("/api/udp", settings);
  }

  async testSend(): Promise<UdpTestResult> {
    return this.client.post<UdpTestResult>("/api/udp/test");
  }
}

export class DeviceCompensationApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSettings(): Promise<CompensationSettings> {
    return this.client.get<CompensationSettings>("/api/compensation");
  }

  async updateSettings(settings: CompensationSettings): Promise<CompensationSettings> {
    return this.client.post<CompensationSettings>("/api/compensation", settings);
  }
}
