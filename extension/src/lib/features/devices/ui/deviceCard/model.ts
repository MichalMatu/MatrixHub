import {
  normalizeBluetoothMac,
  type BleDevice,
  type BleStatus,
  type ShellyDevice,
} from "@matrixhub/device-sdk";
import type { RealtimeConnectionState } from "$lib/features/realtime/socket/deviceOverviewSocket";
import {
  formatBatteryValue,
  formatCompactElapsedAgo,
  formatCompactHumidityValue,
  formatCompactTemperatureValue,
  formatHumidityValue,
  formatTemperatureValue,
} from "$lib/i18n/formatters";
import type { I18nRuntime } from "$lib/i18n/runtime";

export interface BleThermometerViewModel {
  id: string;
  label: string;
  mac: string;
  reading: BleDevice | null;
}

function findBleReading(mac: string, devices: BleDevice[]) {
  const normalizedMac = normalizeBluetoothMac(mac);
  return (
    devices.find(
      (device) => normalizeBluetoothMac(device.mac) === normalizedMac,
    ) ?? null
  );
}

export function realtimeStatusLabel(
  i18n: I18nRuntime,
  state: RealtimeConnectionState,
) {
  switch (state) {
    case "connected":
      return i18n.t("deviceCard.realtime.active");
    case "reconnecting":
      return i18n.t("deviceCard.realtime.reconnecting");
    case "connecting":
      return i18n.t("deviceCard.realtime.starting");
    case "closed":
      return i18n.t("deviceCard.realtime.paused");
    default:
      return i18n.t("deviceCard.realtime.ready");
  }
}

export function createBleThermometers(
  bleStatus: BleStatus | null,
): BleThermometerViewModel[] {
  const bleSensors = bleStatus?.settings?.sensors ?? [];
  const bleDevices = bleStatus?.devices ?? [];

  if (bleSensors.length > 0) {
    return bleSensors.map((sensor) => ({
      id: sensor.mac,
      label: sensor.alias,
      mac: sensor.mac,
      reading: findBleReading(sensor.mac, bleDevices),
    }));
  }

  return bleDevices.map((device) => ({
    id: device.mac,
    label: device.mac,
    mac: device.mac,
    reading: device,
  }));
}

export function formatBleTemperature(
  i18n: I18nRuntime,
  value: number | undefined,
) {
  return typeof value === "number"
    ? formatTemperatureValue(i18n, value)
    : i18n.t("common.waiting");
}

export function formatBleHumidity(
  i18n: I18nRuntime,
  value: number | undefined,
) {
  return typeof value === "number"
    ? formatHumidityValue(i18n, value)
    : i18n.t("common.waiting");
}

export function formatBleBattery(i18n: I18nRuntime, value: number | undefined) {
  return typeof value === "number"
    ? formatBatteryValue(i18n, value)
    : i18n.t("common.waiting");
}

function formatBleCompactTemperature(
  i18n: I18nRuntime,
  value: number | undefined,
) {
  return typeof value === "number"
    ? formatCompactTemperatureValue(i18n, value)
    : "--";
}

function formatBleCompactHumidity(
  i18n: I18nRuntime,
  value: number | undefined,
) {
  return typeof value === "number"
    ? formatCompactHumidityValue(i18n, value)
    : "--";
}

export function getBleSummaryReading(
  i18n: I18nRuntime,
  reading: BleDevice | null,
) {
  return `${formatBleCompactTemperature(i18n, reading?.temp)} ${formatBleCompactHumidity(i18n, reading?.humid)}`;
}

export function formatBleLastSeen(
  i18n: I18nRuntime,
  timestamp: number | undefined,
  now = Date.now(),
) {
  if (!timestamp) {
    return i18n.t("common.waiting");
  }

  return formatCompactElapsedAgo(i18n, timestamp, now);
}

export function getBleIndicatorState(
  timestamp: number | undefined,
  now = Date.now(),
) {
  if (!timestamp) {
    return "waiting";
  }

  const ageMinutes = Math.floor(Math.max(0, now - timestamp) / 60000);
  if (ageMinutes < 2) {
    return "fresh";
  }
  if (ageMinutes < 15) {
    return "recent";
  }

  return "stale";
}

export function getBleIndicatorLabel(
  i18n: I18nRuntime,
  timestamp: number | undefined,
) {
  if (!timestamp) {
    return i18n.t("deviceCard.ble.noRecentSample");
  }

  return i18n.t("deviceCard.ble.lastSample", {
    time: formatBleLastSeen(i18n, timestamp),
  });
}

export function getBleSummaryLabel(
  i18n: I18nRuntime,
  label: string,
  reading: BleDevice | null,
) {
  return i18n.t("deviceCard.ble.summaryLabel", {
    label,
    reading: getBleSummaryReading(i18n, reading),
    status: getBleIndicatorLabel(i18n, reading?.last_seen),
  });
}

export function getShellyStateLabel(i18n: I18nRuntime, device: ShellyDevice) {
  if (!device.isOnline) {
    return `${device.isOn ? i18n.t("deviceCard.shelly.on") : i18n.t("deviceCard.shelly.off")} • ${i18n.t("deviceCard.shelly.offlineSuffix")}`;
  }

  return device.isOn
    ? i18n.t("deviceCard.shelly.on")
    : i18n.t("deviceCard.shelly.off");
}

export function getShellyStateTone(
  pendingShellyDeviceId: string | null,
  device: ShellyDevice,
) {
  if (pendingShellyDeviceId === device.id) {
    return "pending";
  }

  if (!device.isOnline) {
    return "offline";
  }

  return device.isOn ? "on" : "off";
}

export function getShellyStateCopy(
  i18n: I18nRuntime,
  pendingShellyDeviceId: string | null,
  device: ShellyDevice,
) {
  return pendingShellyDeviceId === device.id
    ? i18n.t("deviceCard.shelly.queued")
    : getShellyStateLabel(i18n, device);
}

export function formatBrightnessValue(value: number) {
  return `${Math.min(32, Math.max(0, Math.round(value)))}`;
}
