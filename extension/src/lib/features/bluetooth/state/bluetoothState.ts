import {
  normalizeBluetoothMac,
  type BleDevice,
  type BleDeviceEvent,
  type BleSettings,
  type BleStatus,
} from "@matrixhub/device-sdk";

function isSavedDevice(settings: BleSettings | undefined, mac: string) {
  const normalizedMac = normalizeBluetoothMac(mac);
  return (
    settings?.sensors?.some(
      (sensor) => normalizeBluetoothMac(sensor.mac) === normalizedMac,
    ) ?? false
  );
}

function hydrateSavedFlag(
  device: BleDevice,
  settings: BleSettings | undefined,
): BleDevice {
  const derivedSaved = isSavedDevice(settings, device.mac);
  return {
    ...device,
    mac: normalizeBluetoothMac(device.mac),
    saved: derivedSaved || device.saved,
  };
}

function sortDevices(devices: BleDevice[]) {
  return [...devices].sort(
    (left, right) =>
      Number(Boolean(right.saved)) - Number(Boolean(left.saved)) ||
      right.last_seen - left.last_seen ||
      left.mac.localeCompare(right.mac),
  );
}

function hydrateDevices(
  devices: BleDevice[] | undefined,
  settings: BleSettings | undefined,
) {
  if (!devices) {
    return undefined;
  }

  return sortDevices(
    devices.map((device) => hydrateSavedFlag(device, settings)),
  );
}

export function applyBleStatusSnapshot(
  current: BleStatus | null,
  next: BleStatus,
): BleStatus {
  const settings = next.settings ?? current?.settings;
  const devices =
    next.devices !== undefined
      ? hydrateDevices(next.devices, settings)
      : hydrateDevices(current?.devices, settings);

  return {
    ...current,
    ...next,
    ...(settings ? { settings } : {}),
    ...(devices
      ? { devices }
      : next.devices !== undefined
        ? { devices: [] }
        : {}),
  };
}

export function applyBleDeviceEvent(
  current: BleStatus | null,
  event: BleDeviceEvent,
): BleStatus {
  const settings = current?.settings;
  const nextDevice = hydrateSavedFlag(
    {
      mac: event.mac,
      temp: event.temp,
      humid: event.humid,
      batt: event.batt,
      rssi: event.rssi,
      last_seen: event.lastSeen,
    },
    settings,
  );

  const devices = current?.devices ?? [];
  const normalizedMac = normalizeBluetoothMac(event.mac);
  const nextDevices = sortDevices([
    ...devices.filter(
      (device) => normalizeBluetoothMac(device.mac) !== normalizedMac,
    ),
    nextDevice,
  ]);

  return {
    enabled: current?.enabled ?? false,
    running: current?.running ?? false,
    ...(current?.scanner_active !== undefined
      ? { scanner_active: current.scanner_active }
      : {}),
    ...(settings ? { settings } : {}),
    devices: nextDevices,
  };
}
