import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type { BleSettings, BleStatus } from "./types";
import { parseBleSettings, parseBleStatusSnapshot } from "./ws";

const BLE_SCAN_TIMEOUT_MIN_MS = 1000;
const BLE_SCAN_TIMEOUT_MAX_MS = 300000;
const BLE_SCAN_TIMEOUT_DEFAULT_MS = 30000;

function clampBleScanTimeout(timeoutMs: number) {
  if (!Number.isFinite(timeoutMs)) {
    return BLE_SCAN_TIMEOUT_DEFAULT_MS;
  }

  return Math.min(
    BLE_SCAN_TIMEOUT_MAX_MS,
    Math.max(BLE_SCAN_TIMEOUT_MIN_MS, Math.round(timeoutMs)),
  );
}

function requireParsed<T>(value: T | null, message: string) {
  if (!value) {
    throw new Error(message);
  }

  return value;
}

export function normalizeBluetoothMac(input: string | null | undefined) {
  if (!input) {
    return "";
  }

  return input.trim().toLowerCase();
}

export class DeviceBleApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getStatus(): Promise<BleStatus> {
    const response = await this.client.get<unknown>("/api/ble/status");
    return requireParsed(
      parseBleStatusSnapshot(response),
      "ble/status_invalid",
    );
  }

  async getSettings(): Promise<BleSettings> {
    const response = await this.client.get<unknown>("/api/ble/settings");
    return requireParsed(
      parseBleSettings(response),
      "ble/settings_invalid",
    );
  }

  async saveSettings(settings: Partial<BleSettings>): Promise<BleSettings> {
    const response = await this.client.post<unknown>("/api/ble/settings", settings);
    return requireParsed(
      parseBleSettings(response),
      "ble/settings_invalid",
    );
  }

  async startScan(timeoutMs = BLE_SCAN_TIMEOUT_DEFAULT_MS): Promise<void> {
    const timeout = clampBleScanTimeout(timeoutMs);
    await this.client.postVoid(`/api/ble/scan?timeout=${timeout}`, {});
  }

  async stopScan(): Promise<void> {
    await this.client.deleteVoid("/api/ble/scan");
  }
}
