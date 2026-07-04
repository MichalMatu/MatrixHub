import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type { ShellyDevice } from "./types";
import { parseShellySnapshot } from "./ws";

function requireParsed<T>(value: T | null, message: string) {
  if (!value) {
    throw new Error(message);
  }

  return value;
}

export class DeviceShellyApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getDevices(): Promise<ShellyDevice[]> {
    const response = await this.client.get<unknown>("/api/shelly/devices");
    return requireParsed(
      parseShellySnapshot(response),
      "shelly/devices_invalid",
    );
  }

  async setRelayState(id: string, on: boolean): Promise<void> {
    await this.client.postVoid("/api/shelly/control", {
      id,
      on,
    });
  }
}
