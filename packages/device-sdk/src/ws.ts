import type {
  BleDevice,
  BleDeviceEvent,
  BleSettings,
  BleStatus,
  ShellyDevice,
  ShellyDeviceEvent,
  SensorTelemetryEvent,
  StorageMetricsInfo,
  SystemInformation,
  SystemStatus,
  SystemStatusApDiagnostics,
  SystemStatusDashboardWidgetsSummary,
  SystemStatusForwardingDiagnostics,
  SystemStatusHttpDiagnostics,
  SystemStatusSnapshot,
  SystemStatusWifiDiagnostics,
  SystemStorageInfo,
  TelemetryHistorySnapshot,
  TelemetrySnapshot
} from "./types";
import { normalizeDeviceOrigin } from "./device";

export interface RealtimeEnvelopeFrame {
  type?: string;
  channel?: string;
  data?: unknown;
}

function isObject(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object";
}

function optionalString(value: unknown) {
  return typeof value === "string" ? value : undefined;
}

function optionalNumber(value: unknown) {
  return typeof value === "number" && Number.isFinite(value) ? value : undefined;
}

function optionalBoolean(value: unknown) {
  return typeof value === "boolean" ? value : undefined;
}

function normalizeBluetoothMac(input: string) {
  return input.trim().toLowerCase();
}

function sanitizeTelemetryHistory(value: unknown): TelemetryHistorySnapshot | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const history: TelemetryHistorySnapshot = {};

  if (Array.isArray(value.timestamps) && value.timestamps.every((entry) => typeof entry === "number")) {
    history.timestamps = [...value.timestamps];
  }

  if (
    Array.isArray(value.co2) &&
    value.co2.every((entry) => entry === null || typeof entry === "number")
  ) {
    history.co2 = [...value.co2];
  }

  if (
    Array.isArray(value.temp) &&
    value.temp.every((entry) => entry === null || typeof entry === "number")
  ) {
    history.temp = [...value.temp];
  }

  if (
    Array.isArray(value.humid) &&
    value.humid.every((entry) => entry === null || typeof entry === "number")
  ) {
    history.humid = [...value.humid];
  }

  return Object.keys(history).length > 0 ? history : undefined;
}

export function parseTelemetrySnapshot(value: unknown): TelemetrySnapshot | null {
  if (!isObject(value)) {
    return null;
  }

  const co2 = optionalNumber(value.co2);
  const temp = optionalNumber(value.temp);
  const humid = optionalNumber(value.humid);
  if (co2 === undefined || temp === undefined || humid === undefined) {
    return null;
  }

  const snapshot: TelemetrySnapshot = {
    co2,
    temp,
    humid
  };

  const lastReadOk = optionalBoolean(value.lastReadOk);
  if (lastReadOk !== undefined) {
    snapshot.lastReadOk = lastReadOk;
  }

  const history = sanitizeTelemetryHistory(value.history);
  if (history) {
    snapshot.history = history;
  }

  return snapshot;
}

export function parseSensorTelemetryEvent(value: unknown): SensorTelemetryEvent | null {
  if (!isObject(value)) {
    return null;
  }

  const co2 = optionalNumber(value.co2);
  const temp = optionalNumber(value.temp);
  const humid = optionalNumber(value.humid);
  const lastReadOk = optionalBoolean(value.lastReadOk);
  const timestampMs = optionalNumber(value.timestamp_ms);
  if (
    co2 === undefined ||
    temp === undefined ||
    humid === undefined ||
    lastReadOk === undefined ||
    timestampMs === undefined
  ) {
    return null;
  }

  return {
    co2,
    temp,
    humid,
    lastReadOk,
    timestamp_ms: timestampMs
  };
}

export function parseSensorTelemetryPacket(buffer: ArrayBuffer): SensorTelemetryEvent | null {
  if (buffer.byteLength < 11 || new DataView(buffer).getUint8(0) !== 0x54) {
    return null;
  }

  const view = new DataView(buffer);
  return {
    co2: view.getUint16(1, true),
    temp: view.getInt16(3, true) / 10,
    humid: view.getUint16(5, true) / 10,
    timestamp_ms: view.getUint32(7, true),
    // Newer firmware appends one health/status byte. Older 11-byte packets are
    // still valid live telemetry and imply a successful read.
    lastReadOk: buffer.byteLength >= 12 ? (view.getUint8(11) & 0x01) !== 0 : true
  };
}

function sanitizeBleSensorConfig(
  value: unknown
): NonNullable<BleSettings["sensors"]>[number] | null {
  if (!isObject(value)) {
    return null;
  }

  const mac = optionalString(value.mac);
  const alias = optionalString(value.alias);
  if (!mac || !alias) {
    return null;
  }

  return {
    mac: normalizeBluetoothMac(mac),
    alias
  };
}

function sanitizeBleDevice(value: unknown): BleDevice | null {
  if (!isObject(value)) {
    return null;
  }

  const mac = optionalString(value.mac);
  const temp = optionalNumber(value.temp);
  const humid = optionalNumber(value.humid);
  const batt = optionalNumber(value.batt);
  const rssi = optionalNumber(value.rssi);
  const lastSeen = optionalNumber(value.last_seen);
  if (
    !mac ||
    temp === undefined ||
    humid === undefined ||
    batt === undefined ||
    rssi === undefined ||
    lastSeen === undefined
  ) {
    return null;
  }

  const device: BleDevice = {
    mac: normalizeBluetoothMac(mac),
    temp,
    humid,
    batt,
    rssi,
    last_seen: lastSeen
  };

  const saved = optionalBoolean(value.saved);
  if (saved !== undefined) {
    device.saved = saved;
  }

  return device;
}

function sanitizeBleMetrics(value: unknown): BleStatus["metrics"] | null {
  if (!isObject(value)) {
    return null;
  }

  const advTotal = optionalNumber(value.adv_total);
  const validPackets = optionalNumber(value.valid_packets);
  const parserErrors = optionalNumber(value.parser_errors);
  const cacheDrops = optionalNumber(value.cache_drops);
  const mutexTimeouts = optionalNumber(value.mutex_timeouts);
  const scannerRunning = optionalBoolean(value.scanner_running);

  if (
    advTotal === undefined ||
    validPackets === undefined ||
    parserErrors === undefined ||
    cacheDrops === undefined ||
    mutexTimeouts === undefined ||
    scannerRunning === undefined
  ) {
    return null;
  }

  return {
    adv_total: advTotal,
    valid_packets: validPackets,
    parser_errors: parserErrors,
    cache_drops: cacheDrops,
    mutex_timeouts: mutexTimeouts,
    scanner_running: scannerRunning
  };
}

export function parseBleSettings(value: unknown): BleSettings | null {
  if (!isObject(value)) {
    return null;
  }

  const enabled = optionalBoolean(value.enabled);
  if (enabled === undefined) {
    return null;
  }

  const sensors = Array.isArray(value.sensors)
    ? value.sensors
        .map(sanitizeBleSensorConfig)
        .filter((entry): entry is NonNullable<BleSettings["sensors"]>[number] => entry !== null)
    : [];

  return {
    enabled,
    ...(sensors.length > 0 ? { sensors } : {})
  };
}

export function parseBleStatusSnapshot(value: unknown): BleStatus | null {
  if (!isObject(value)) {
    return null;
  }

  const enabled = optionalBoolean(value.enabled);
  const running = optionalBoolean(value.running);
  if (enabled === undefined || running === undefined) {
    return null;
  }

  const status: BleStatus = {
    enabled,
    running
  };

  const scannerActive = optionalBoolean(value.scanner_active);
  if (scannerActive !== undefined) {
    status.scanner_active = scannerActive;
  }

  const settings = parseBleSettings(value.settings);
  if (settings) {
    status.settings = settings;
  }

  const metrics = sanitizeBleMetrics(value.metrics);
  if (metrics) {
    status.metrics = metrics;
  }

  if (Array.isArray(value.devices)) {
    status.devices = value.devices
      .map(sanitizeBleDevice)
      .filter((entry): entry is BleDevice => entry !== null);
  }

  return status;
}

export function parseBleEventPacket(
  buffer: ArrayBuffer,
  receivedAt = Date.now()
): BleDeviceEvent | null {
  if (buffer.byteLength < 13 || new DataView(buffer).getUint8(0) !== 0x42) {
    return null;
  }

  const view = new DataView(buffer);
  const macBytes = new Uint8Array(buffer.slice(1, 7));
  const mac = normalizeBluetoothMac(
    Array.from(macBytes)
      .map((byte) => byte.toString(16).padStart(2, "0"))
      .join(":")
  );

  return {
    mac,
    temp: view.getInt16(7, true) / 10,
    humid: view.getUint16(9, true) / 10,
    batt: view.getUint8(11),
    rssi: view.getInt8(12),
    lastSeen: receivedAt
  };
}

function sanitizeShellyDevice(value: unknown): ShellyDevice | null {
  if (!isObject(value)) {
    return null;
  }

  const id = optionalString(value.id);
  const name = optionalString(value.name);
  const isOn = optionalBoolean(value.isOn);
  const isOnline = optionalBoolean(value.isOnline);
  if (!id || !name || isOn === undefined || isOnline === undefined) {
    return null;
  }

  const device: ShellyDevice = {
    id,
    name,
    isOn,
    isOnline
  };

  const ip = optionalString(value.ip);
  if (ip) {
    device.ip = ip;
  }

  const relayIndex = optionalNumber(value.relay_index);
  if (relayIndex !== undefined) {
    device.relayIndex = relayIndex;
  }

  const enabled = optionalBoolean(value.enabled);
  if (enabled !== undefined) {
    device.enabled = enabled;
  }

  const generation = optionalNumber(value.generation);
  if (generation !== undefined) {
    device.generation = generation;
  }

  const lastUpdate = optionalNumber(value.lastUpdate);
  if (lastUpdate !== undefined) {
    device.lastUpdate = lastUpdate;
  }

  const power = optionalNumber(value.power);
  if (power !== undefined) {
    device.power = power;
  }

  const energy = optionalNumber(value.energy);
  if (energy !== undefined) {
    device.energy = energy;
  }

  const voltage = optionalNumber(value.voltage);
  if (voltage !== undefined) {
    device.voltage = voltage;
  }

  const current = optionalNumber(value.current);
  if (current !== undefined) {
    device.current = current;
  }

  const temp = optionalNumber(value.temp);
  if (temp !== undefined) {
    device.temp = temp;
  }

  const rssi = optionalNumber(value.rssi);
  if (rssi !== undefined) {
    device.rssi = rssi;
  }

  return device;
}

export function parseShellySnapshot(value: unknown): ShellyDevice[] | null {
  if (!Array.isArray(value)) {
    return null;
  }

  return value
    .map(sanitizeShellyDevice)
    .filter((entry): entry is ShellyDevice => entry !== null);
}

export function parseShellyEventPacket(buffer: ArrayBuffer): ShellyDeviceEvent | null {
  if (buffer.byteLength < 15 || new DataView(buffer).getUint8(0) !== 0x53) {
    return null;
  }

  const view = new DataView(buffer);
  const idLength = view.getUint8(1);
  const minimumLength = 15 + idLength;
  if (idLength === 0 || idLength > 8 || buffer.byteLength < minimumLength) {
    return null;
  }

  const idBytes = new Uint8Array(buffer.slice(2, 2 + idLength));
  const id = Array.from(idBytes, (byte) => String.fromCharCode(byte)).join("");
  if (!id) {
    return null;
  }

  let offset = 2 + idLength;
  const flags = view.getUint8(offset++);
  const power = view.getUint16(offset, true) / 100;
  offset += 2;
  const voltage = view.getUint16(offset, true) / 10;
  offset += 2;
  const current = view.getUint16(offset, true) / 1000;
  offset += 2;
  const energy = view.getUint32(offset, true) / 10;
  offset += 4;
  const temp = view.getInt8(offset++);
  const rssi = view.getInt8(offset);

  return {
    id,
    isOnline: (flags & 0x01) !== 0,
    isOn: (flags & 0x02) !== 0,
    power,
    voltage,
    current,
    energy,
    temp,
    rssi
  };
}

function sanitizeStorageMetricsInfo(value: unknown): StorageMetricsInfo | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const backend = optionalString(value.backend);
  const available = optionalBoolean(value.available);
  const mounted = optionalBoolean(value.mounted);
  const totalBytes = optionalNumber(value.total_bytes);
  const usedBytes = optionalNumber(value.used_bytes);
  const freeBytes = optionalNumber(value.free_bytes);
  if (
    backend === undefined ||
    available === undefined ||
    mounted === undefined ||
    totalBytes === undefined ||
    usedBytes === undefined ||
    freeBytes === undefined
  ) {
    return undefined;
  }

  return {
    backend,
    available,
    mounted,
    total_bytes: totalBytes,
    used_bytes: usedBytes,
    free_bytes: freeBytes,
    last_error: optionalString(value.last_error)
  };
}

function sanitizeSystemStorageInfo(value: unknown): SystemStorageInfo | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const active = sanitizeStorageMetricsInfo(value.active);
  const littlefs = sanitizeStorageMetricsInfo(value.littlefs);
  const sdcard = sanitizeStorageMetricsInfo(value.sdcard);
  const filesystem = isObject(value.filesystem)
    ? {
        total_bytes: optionalNumber(value.filesystem.total_bytes),
        used_bytes: optionalNumber(value.filesystem.used_bytes),
        free_bytes: optionalNumber(value.filesystem.free_bytes)
      }
    : null;

  if (
    !active ||
    !littlefs ||
    !sdcard ||
    !filesystem?.total_bytes ||
    filesystem.used_bytes === undefined ||
    filesystem.free_bytes === undefined
  ) {
    return undefined;
  }

  const activeBackend = optionalString(value.active_backend);
  const activePath = optionalString(value.active_path);
  if (!activeBackend || !activePath) {
    return undefined;
  }

  return {
    filesystem: {
      total_bytes: filesystem.total_bytes,
      used_bytes: filesystem.used_bytes,
      free_bytes: filesystem.free_bytes
    },
    active_backend: activeBackend,
    active_path: activePath,
    active,
    littlefs,
    sdcard
  };
}

const SYSTEM_INFO_NUMBER_KEYS = [
  "cpu_freq_mhz",
  "cpu_rev",
  "cpu_cores",
  "sketch_size",
  "free_sketch_space",
  "flash_chip_size",
  "flash_chip_speed",
  "cpu_reset_reason",
  "max_alloc_heap",
  "psram_size",
  "free_psram",
  "used_psram",
  "free_heap",
  "used_heap",
  "total_heap",
  "min_free_heap",
  "core_temp",
  "fs_total",
  "fs_used",
  "lp_sram_used",
  "lp_sram_free",
  "lp_sram_total",
  "uptime"
] as const;

const SYSTEM_INFO_STRING_KEYS = [
  "esp_platform",
  "firmware_version",
  "firmware_name",
  "firmware_built_target",
  "cpu_type",
  "sdk_version",
  "arduino_version",
  "mac_address",
  "compile_date",
  "compile_time"
] as const;

function sanitizeSystemInformation(value: unknown): SystemInformation | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const info: Partial<SystemInformation> = {};

  SYSTEM_INFO_NUMBER_KEYS.forEach((key) => {
    const parsed = optionalNumber(value[key]);
    if (parsed !== undefined) {
      info[key] = parsed as never;
    }
  });

  SYSTEM_INFO_STRING_KEYS.forEach((key) => {
    const parsed = optionalString(value[key]);
    if (parsed !== undefined) {
      info[key] = parsed as never;
    }
  });

  const storage = sanitizeSystemStorageInfo(value.storage);
  if (storage) {
    info.storage = storage;
  }

  return Object.keys(info).length > 0 ? (info as SystemInformation) : undefined;
}

function sanitizeWifiDiagnostics(value: unknown): SystemStatusWifiDiagnostics | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const diagnostics: SystemStatusWifiDiagnostics = {};
  const numberKeys = [
    "lastDisconnectReason",
    "lastIpChangeMs",
    "disconnectedSinceMs",
    "stableConnectedSinceMs",
    "channel",
    "rssi"
  ] as const;
  const stringKeys = [
    "state",
    "mode",
    "rescueReason",
    "lastRecoveryReason",
    "ssid",
    "ip",
    "savedStaticIp",
    "mac",
    "bssid",
    "gateway",
    "subnet",
    "dns"
  ] as const;
  const booleanKeys = ["connected", "healthy", "rescueApActive"] as const;

  numberKeys.forEach((key) => {
    const parsed = optionalNumber(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  stringKeys.forEach((key) => {
    const parsed = optionalString(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  booleanKeys.forEach((key) => {
    const parsed = optionalBoolean(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  return Object.keys(diagnostics).length > 0 ? diagnostics : undefined;
}

function sanitizeApDiagnostics(value: unknown): SystemStatusApDiagnostics | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const diagnostics: SystemStatusApDiagnostics = {};
  const numberKeys = ["stationNum"] as const;
  const stringKeys = ["mode", "ip", "mac"] as const;
  const booleanKeys = ["active"] as const;

  numberKeys.forEach((key) => {
    const parsed = optionalNumber(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  stringKeys.forEach((key) => {
    const parsed = optionalString(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  booleanKeys.forEach((key) => {
    const parsed = optionalBoolean(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  return Object.keys(diagnostics).length > 0 ? diagnostics : undefined;
}

function sanitizeHttpDiagnostics(value: unknown): SystemStatusHttpDiagnostics | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const diagnostics: SystemStatusHttpDiagnostics = {};
  const numberKeys = [
    "activeClients",
    "peakClients",
    "opens",
    "closes",
    "lastOpenMs",
    "lastCloseMs",
    "wsActiveClients",
    "wsPeakClients",
    "wsOpens",
    "wsCloses",
    "lastWsOpenMs",
    "lastWsCloseMs",
    "wsForcedRemovals",
    "wsQueueDrops",
    "lastWsQueueDropMs",
    "lastWsQueueDropPayload",
    "wsHeapFallbacks",
    "lastWsHeapFallbackMs",
    "lastWsHeapFallbackPayload",
    "maxWsHeapFallbackPayload"
  ] as const;

  numberKeys.forEach((key) => {
    const parsed = optionalNumber(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  return Object.keys(diagnostics).length > 0 ? diagnostics : undefined;
}

function sanitizeForwardingDiagnostics(
  value: unknown
): SystemStatusForwardingDiagnostics | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  const diagnostics: SystemStatusForwardingDiagnostics = {};
  const numberKeys = ["httpsPort"] as const;
  const booleanKeys = [
    "ready",
    "requiresStaticIp",
    "savedStaticIpConfigured",
    "savedStaticIpMatches"
  ] as const;

  numberKeys.forEach((key) => {
    const parsed = optionalNumber(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  booleanKeys.forEach((key) => {
    const parsed = optionalBoolean(value[key]);
    if (parsed !== undefined) {
      diagnostics[key] = parsed as never;
    }
  });

  return Object.keys(diagnostics).length > 0 ? diagnostics : undefined;
}

function sanitizeDashboardWidgets(
  value: unknown
): SystemStatusDashboardWidgetsSummary | undefined {
  if (!isObject(value)) {
    return undefined;
  }

  if (
    !isObject(value.ble) ||
    !isObject(value.shelly) ||
    !isObject(value.alarms) ||
    !isObject(value.wifi_sensing)
  ) {
    return undefined;
  }

  const bleEnabled = optionalBoolean(value.ble.enabled);
  const bleSensorCount = optionalNumber(value.ble.sensor_count);
  const shellyDeviceCount = optionalNumber(value.shelly.device_count);
  const alarmRuleCount = optionalNumber(value.alarms.rule_count);
  const wifiSensingEnabled = optionalBoolean(value.wifi_sensing.enabled);
  if (
    bleEnabled === undefined ||
    bleSensorCount === undefined ||
    shellyDeviceCount === undefined ||
    alarmRuleCount === undefined ||
    wifiSensingEnabled === undefined
  ) {
    return undefined;
  }

  return {
    ble: {
      enabled: bleEnabled,
      sensor_count: bleSensorCount
    },
    shelly: {
      device_count: shellyDeviceCount
    },
    alarms: {
      rule_count: alarmRuleCount
    },
    wifi_sensing: {
      enabled: wifiSensingEnabled
    }
  };
}

export function parseSystemStatusSnapshot(value: unknown): SystemStatusSnapshot | null {
  if (!isObject(value)) {
    return null;
  }

  const snapshot: SystemStatusSnapshot = {};
  const systemInfo = sanitizeSystemInformation(value.system_info);
  if (systemInfo) {
    snapshot.system_info = systemInfo;
  }

  if (isObject(value.diagnostics)) {
    const diagnostics: NonNullable<SystemStatusSnapshot["diagnostics"]> = {};
    const wifi = sanitizeWifiDiagnostics(value.diagnostics.wifi);
    if (wifi) {
      diagnostics.wifi = wifi;
    }

    const ap = sanitizeApDiagnostics(value.diagnostics.ap);
    if (ap) {
      diagnostics.ap = ap;
    }

    const http = sanitizeHttpDiagnostics(value.diagnostics.http);
    if (http) {
      diagnostics.http = http;
    }

    const forwarding = sanitizeForwardingDiagnostics(value.diagnostics.forwarding);
    if (forwarding) {
      diagnostics.forwarding = forwarding;
    }

    if (Object.keys(diagnostics).length > 0) {
      snapshot.diagnostics = diagnostics;
    }
  }

  const dashboardWidgets = sanitizeDashboardWidgets(value.dashboard_widgets);
  if (dashboardWidgets) {
    snapshot.dashboard_widgets = dashboardWidgets;
  }

  const wifiApMode = optionalBoolean(value.wifi_ap_mode);
  if (wifiApMode !== undefined) {
    snapshot.wifi_ap_mode = wifiApMode;
  }

  if (isObject(value.wifi_settings)) {
    const connectionMode = optionalNumber(value.wifi_settings.connection_mode);
    const wifiNetworks = Array.isArray(value.wifi_settings.wifi_networks)
      ? value.wifi_settings.wifi_networks
          .filter(isObject)
          .map((entry) => ({ ssid: optionalString(entry.ssid) }))
          .filter((entry): entry is { ssid: string } => typeof entry.ssid === "string")
      : [];

    if (connectionMode !== undefined || wifiNetworks.length > 0) {
      snapshot.wifi_settings = {};
      if (connectionMode !== undefined) {
        snapshot.wifi_settings.connection_mode = connectionMode;
      }
      if (wifiNetworks.length > 0) {
        snapshot.wifi_settings.wifi_networks = wifiNetworks;
      }
    }
  }

  if (isObject(value.config) && isObject(value.config.logging)) {
    const level = optionalString(value.config.logging.level);
    if (level) {
      snapshot.config = {
        logging: {
          level
        }
      };
    }
  }

  return snapshot;
}

export function parseRealtimeEnvelopeFrame(frame: string): RealtimeEnvelopeFrame | null {
  let parsed: unknown;

  try {
    parsed = JSON.parse(frame);
  } catch {
    return null;
  }

  if (!isObject(parsed)) {
    return null;
  }

  return {
    type: optionalString(parsed.type),
    channel: optionalString(parsed.channel),
    data: parsed.data
  };
}

export function buildDeviceWebSocketUrl(origin: string, path = "/ws/system") {
  const normalized = normalizeDeviceOrigin(origin);
  const normalizedPath = path.startsWith("/") ? path : `/${path}`;
  const url = new URL(normalizedPath, `${normalized.origin}/`);
  url.protocol = normalized.protocol === "https:" ? "wss:" : "ws:";
  return url.toString();
}

export function parseSystemStatusPacket(buffer: ArrayBuffer): SystemStatus | null {
  if (buffer.byteLength < 10) return null;

  const view = new DataView(buffer);
  if (view.getUint8(0) !== 0xa5) return null;

  let offset = 1;
  const timestampSec = view.getUint32(offset, true);
  offset += 4;

  const wifiStatus = view.getUint8(offset++);
  const wifiFlags = view.getUint8(offset++);
  const rssi = view.getInt8(offset++);

  let coreTemp = 0;
  if (view.byteLength >= offset + 2) {
    coreTemp = view.getInt16(offset, true) / 10.0;
  }

  const isStaConnected = (wifiFlags & 0x01) !== 0;
  const isApMode = (wifiFlags & 0x02) !== 0;

  return {
    timestamp: timestampSec * 1000,
    lastUpdate: Date.now(),
    wifiStatus,
    rssi,
    isConnected: isStaConnected || isApMode,
    isStaConnected,
    isApMode,
    coreTemp
  };
}
