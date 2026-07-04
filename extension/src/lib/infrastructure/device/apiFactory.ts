import {
  DeviceBleApi,
  DeviceMatrixApi,
  DeviceShellyApi,
  DeviceSystemApi,
  type DeviceRecord,
  type DeviceSession,
} from "@matrixhub/device-sdk";

interface DeviceApiContext {
  selectedDevice: DeviceRecord | null;
  currentSession: DeviceSession | null;
  onUnauthorized: () => void;
}

export function createSystemApiForSelection(context: DeviceApiContext) {
  if (!context.selectedDevice || !context.currentSession) {
    return null;
  }

  return new DeviceSystemApi({
    baseUrl: context.selectedDevice.origin,
    bearerToken: context.currentSession.accessToken,
    onUnauthorized: context.onUnauthorized,
  });
}

export function createBleApiForSelection(context: DeviceApiContext) {
  if (!context.selectedDevice || !context.currentSession) {
    return null;
  }

  return new DeviceBleApi({
    baseUrl: context.selectedDevice.origin,
    bearerToken: context.currentSession.accessToken,
    onUnauthorized: context.onUnauthorized,
  });
}

export function createShellyApiForSelection(context: DeviceApiContext) {
  if (!context.selectedDevice || !context.currentSession) {
    return null;
  }

  return new DeviceShellyApi({
    baseUrl: context.selectedDevice.origin,
    bearerToken: context.currentSession.accessToken,
    onUnauthorized: context.onUnauthorized,
  });
}

export function createMatrixApiForSelection(context: DeviceApiContext) {
  if (!context.selectedDevice || !context.currentSession) {
    return null;
  }

  return new DeviceMatrixApi({
    baseUrl: context.selectedDevice.origin,
    bearerToken: context.currentSession.accessToken,
    onUnauthorized: context.onUnauthorized,
  });
}
