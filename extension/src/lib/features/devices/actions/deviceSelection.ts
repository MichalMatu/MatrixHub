import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
import {
  getNextSelectedDeviceId,
  removeDeviceRecord,
  removeSessionRecord,
} from "$lib/domain/device/rules";

interface RemoveDeviceContextInput {
  devices: DeviceRecord[];
  sessions: Record<string, DeviceSession>;
  selectedDeviceId: string | null;
  deviceId: string;
}

export function buildRemoveDeviceContext(input: RemoveDeviceContextInput) {
  const removedDevice = input.devices.find(
    (entry) => entry.id === input.deviceId,
  );
  if (!removedDevice) {
    return null;
  }

  const hadSession = !!input.sessions[input.deviceId];
  const nextSessions = hadSession
    ? removeSessionRecord(input.sessions, input.deviceId)
    : input.sessions;
  const nextDevices = removeDeviceRecord(input.devices, input.deviceId);

  return {
    removedDevice,
    hadSession,
    devices: nextDevices,
    sessions: nextSessions,
    selectedDeviceId: getNextSelectedDeviceId(
      nextDevices,
      input.selectedDeviceId,
      input.deviceId,
    ),
  };
}

export function clearSelectedSessionContext(
  sessions: Record<string, DeviceSession>,
  selectedDeviceId: string | null,
) {
  if (!selectedDeviceId) {
    return sessions;
  }

  return removeSessionRecord(sessions, selectedDeviceId);
}
