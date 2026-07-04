import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";

export function upsertDeviceRecord(
  devices: DeviceRecord[],
  nextDevice: DeviceRecord,
) {
  const existing = devices.find((device) => device.id === nextDevice.id);
  if (!existing) {
    return [...devices, nextDevice];
  }

  return devices.map((device) =>
    device.id === nextDevice.id ? nextDevice : device,
  );
}

export function markDeviceConnected(
  devices: DeviceRecord[],
  deviceId: string,
  connectedAt: string,
) {
  return devices.map((device) =>
    device.id === deviceId
      ? {
          ...device,
          lastConnectedAt: connectedAt,
        }
      : device,
  );
}

export function renameDeviceRecord(
  devices: DeviceRecord[],
  deviceId: string,
  name: string,
) {
  return devices.map((device) =>
    device.id === deviceId
      ? {
          ...device,
          name,
        }
      : device,
  );
}

export function removeDeviceRecord(devices: DeviceRecord[], deviceId: string) {
  return devices.filter((device) => device.id !== deviceId);
}

export function removeSessionRecord(
  sessions: Record<string, DeviceSession>,
  deviceId: string,
) {
  const nextSessions = { ...sessions };
  delete nextSessions[deviceId];
  return nextSessions;
}

export function getNextSelectedDeviceId(
  devices: DeviceRecord[],
  selectedDeviceId: string | null,
  removedDeviceId: string,
) {
  return selectedDeviceId === removedDeviceId
    ? (devices[0]?.id ?? null)
    : selectedDeviceId;
}
