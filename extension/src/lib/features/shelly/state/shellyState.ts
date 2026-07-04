import type { ShellyDevice, ShellyDeviceEvent } from "@matrixhub/device-sdk";

function compareShellyDevices(left: ShellyDevice, right: ShellyDevice) {
  return (
    left.name.localeCompare(right.name, undefined, { sensitivity: "base" }) ||
    left.id.localeCompare(right.id)
  );
}

function sortShellyDevices(devices: ShellyDevice[]) {
  return [...devices].sort(compareShellyDevices);
}

export function resolveShellyEventDeviceId(
  devices: ShellyDevice[],
  eventId: string,
) {
  const exactMatch = devices.find((device) => device.id === eventId);
  if (exactMatch) {
    return exactMatch.id;
  }

  const prefixMatches = devices.filter((device) =>
    device.id.startsWith(eventId),
  );

  return prefixMatches.length === 1 ? prefixMatches[0].id : null;
}

export function applyShellySnapshot(snapshot: ShellyDevice[]) {
  return sortShellyDevices(snapshot);
}

export function applyShellyEvent(
  current: ShellyDevice[],
  event: ShellyDeviceEvent,
) {
  const targetDeviceId = resolveShellyEventDeviceId(current, event.id);
  if (!targetDeviceId) {
    return current;
  }

  const nextDevices = current.map((device) =>
    device.id === targetDeviceId
      ? {
          ...device,
          ...event,
          id: device.id,
        }
      : device,
  );

  return sortShellyDevices(nextDevices);
}

export function applyShellyOptimisticState(
  current: ShellyDevice[],
  deviceId: string,
  isOn: boolean,
) {
  return current.map((device) =>
    device.id === deviceId
      ? {
          ...device,
          isOn,
        }
      : device,
  );
}
