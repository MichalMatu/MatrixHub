import type {
  BleStatus,
  MatrixSettings,
  ShellyDevice,
} from "@matrixhub/device-sdk";
import type { OverviewState } from "$lib/features/overview/state/overviewState";

export interface DeviceDataCacheSnapshot {
  overviewState: OverviewState;
  bleStatus: BleStatus | null;
  shellyDevices: ShellyDevice[];
  matrixSettings: MatrixSettings | null;
}

export type DeviceDataOrigin = "empty" | "cache" | "live";

export interface SelectedDeviceDataOrigins {
  overview: DeviceDataOrigin;
  telemetry: DeviceDataOrigin;
  ble: DeviceDataOrigin;
  shelly: DeviceDataOrigin;
  matrix: DeviceDataOrigin;
}

export function createEmptySelectedDeviceDataOrigins(): SelectedDeviceDataOrigins {
  return {
    overview: "empty",
    telemetry: "empty",
    ble: "empty",
    shelly: "empty",
    matrix: "empty",
  };
}

export function hasCachedTelemetryData(overviewState: OverviewState) {
  return overviewState.telemetrySnapshot !== null;
}

export function hasCachedOverviewData(overviewState: OverviewState) {
  return (
    overviewState.systemInfo !== null ||
    overviewState.systemStatusSnapshot !== null ||
    overviewState.systemStatus !== null
  );
}

export function hasCachedBleData(bleStatus: BleStatus | null) {
  return bleStatus !== null;
}

export function hasCachedShellyData(shellyDevices: ShellyDevice[]) {
  return shellyDevices.length > 0;
}

export function hasCachedMatrixData(matrixSettings: MatrixSettings | null) {
  return matrixSettings !== null;
}

export function hasAnyCachedDeviceData(
  snapshot: DeviceDataCacheSnapshot | null | undefined,
) {
  return Boolean(
    snapshot &&
    (hasCachedTelemetryData(snapshot.overviewState) ||
      hasCachedOverviewData(snapshot.overviewState) ||
      hasCachedBleData(snapshot.bleStatus) ||
      hasCachedShellyData(snapshot.shellyDevices) ||
      hasCachedMatrixData(snapshot.matrixSettings)),
  );
}

export function buildCachedDataOrigins(
  snapshot: DeviceDataCacheSnapshot | null | undefined,
): SelectedDeviceDataOrigins {
  if (!snapshot) {
    return createEmptySelectedDeviceDataOrigins();
  }

  return {
    overview: hasCachedOverviewData(snapshot.overviewState) ? "cache" : "empty",
    telemetry: hasCachedTelemetryData(snapshot.overviewState)
      ? "cache"
      : "empty",
    ble: hasCachedBleData(snapshot.bleStatus) ? "cache" : "empty",
    shelly: hasCachedShellyData(snapshot.shellyDevices) ? "cache" : "empty",
    matrix: hasCachedMatrixData(snapshot.matrixSettings) ? "cache" : "empty",
  };
}
