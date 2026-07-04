import { browser } from "wxt/browser";
import {
  parseMatrixSettings,
  parseBleStatusSnapshot,
  parseShellySnapshot,
  parseSystemStatusSnapshot,
  parseTelemetrySnapshot,
  type DeviceRecord,
  type DeviceSession,
  type SystemStatus,
} from "@matrixhub/device-sdk";
import {
  createEmptyOverviewState,
  type OverviewState,
} from "$lib/features/overview/state/overviewState";
import {
  hasAnyCachedDeviceData,
  type DeviceDataCacheSnapshot,
} from "$lib/features/sidepanel/state/deviceDataCache";
import {
  createDefaultDeviceCardSectionState,
  sanitizeDeviceCardSectionState,
  type DeviceCardSectionState,
} from "$lib/features/sidepanel/state/deviceCardSections";

const STORAGE_KEY = "matrixhub-extension-state";

export interface ExtensionState {
  devices: DeviceRecord[];
  selectedDeviceId: string | null;
  sessions: Record<string, DeviceSession>;
  deviceSnapshots: Record<string, DeviceDataCacheSnapshot>;
  sectionVisibility: DeviceCardSectionState;
}

export const EMPTY_EXTENSION_STATE: ExtensionState = {
  devices: [],
  selectedDeviceId: null,
  sessions: {},
  deviceSnapshots: {},
  sectionVisibility: createDefaultDeviceCardSectionState(),
};

function isObject(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object";
}

function sanitizeDevice(value: unknown): DeviceRecord | null {
  if (!isObject(value)) return null;
  if (
    typeof value.id !== "string" ||
    typeof value.name !== "string" ||
    typeof value.origin !== "string" ||
    typeof value.input !== "string" ||
    typeof value.createdAt !== "string"
  ) {
    return null;
  }

  return {
    id: value.id,
    name: value.name,
    origin: value.origin,
    input: value.input,
    createdAt: value.createdAt,
    lastConnectedAt:
      typeof value.lastConnectedAt === "string"
        ? value.lastConnectedAt
        : undefined,
  };
}

function sanitizeSession(value: unknown): DeviceSession | null {
  if (!isObject(value)) return null;
  if (
    typeof value.accessToken !== "string" ||
    typeof value.username !== "string" ||
    typeof value.admin !== "boolean" ||
    typeof value.signedInAt !== "string"
  ) {
    return null;
  }

  return {
    accessToken: value.accessToken,
    username: value.username,
    admin: value.admin,
    signedInAt: value.signedInAt,
  };
}

function isFiniteNumber(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value);
}

function sanitizeSystemStatus(value: unknown): SystemStatus | null {
  if (!isObject(value)) return null;

  if (
    !isFiniteNumber(value.timestamp) ||
    !isFiniteNumber(value.lastUpdate) ||
    !isFiniteNumber(value.wifiStatus) ||
    !isFiniteNumber(value.rssi) ||
    typeof value.isConnected !== "boolean"
  ) {
    return null;
  }

  return {
    timestamp: value.timestamp,
    lastUpdate: value.lastUpdate,
    wifiStatus: value.wifiStatus,
    rssi: value.rssi,
    isConnected: value.isConnected,
    isStaConnected:
      typeof value.isStaConnected === "boolean"
        ? value.isStaConnected
        : undefined,
    isApMode: typeof value.isApMode === "boolean" ? value.isApMode : undefined,
    coreTemp: isFiniteNumber(value.coreTemp) ? value.coreTemp : undefined,
  };
}

function sanitizeSystemInfo(value: unknown) {
  return (
    parseSystemStatusSnapshot({
      system_info: value,
    })?.system_info ?? null
  );
}

function sanitizeSystemStatusSnapshot(value: unknown) {
  const snapshot = parseSystemStatusSnapshot(value);
  return snapshot && Object.keys(snapshot).length > 0 ? snapshot : null;
}

function sanitizeOverviewState(value: unknown): OverviewState {
  const emptyState = createEmptyOverviewState();
  if (!isObject(value)) {
    return emptyState;
  }

  return {
    telemetrySnapshot: parseTelemetrySnapshot(value.telemetrySnapshot) ?? null,
    systemInfo: sanitizeSystemInfo(value.systemInfo),
    systemStatusSnapshot: sanitizeSystemStatusSnapshot(
      value.systemStatusSnapshot,
    ),
    systemStatus: sanitizeSystemStatus(value.systemStatus),
    lastSuccessfulRefreshAt:
      typeof value.lastSuccessfulRefreshAt === "string"
        ? value.lastSuccessfulRefreshAt
        : null,
    lastTelemetryAt:
      typeof value.lastTelemetryAt === "string" ? value.lastTelemetryAt : null,
  };
}

function sanitizeDeviceSnapshot(
  value: unknown,
): DeviceDataCacheSnapshot | null {
  if (!isObject(value)) return null;

  const snapshot: DeviceDataCacheSnapshot = {
    overviewState: sanitizeOverviewState(value.overviewState),
    bleStatus: parseBleStatusSnapshot(value.bleStatus) ?? null,
    shellyDevices: parseShellySnapshot(value.shellyDevices) ?? [],
    matrixSettings: parseMatrixSettings(value.matrixSettings) ?? null,
  };

  return hasAnyCachedDeviceData(snapshot) ? snapshot : null;
}

export function sanitizeExtensionState(raw: unknown): ExtensionState {
  if (!isObject(raw)) {
    return EMPTY_EXTENSION_STATE;
  }

  // The extension may survive schema changes between reloads. Sanitizing here
  // prevents broken local state from crashing the side panel during bootstrap.
  const devices = Array.isArray(raw.devices)
    ? raw.devices
        .map(sanitizeDevice)
        .filter((value): value is DeviceRecord => value !== null)
    : [];

  const knownDeviceIds = new Set(devices.map((device) => device.id));
  const sessions: Record<string, DeviceSession> = {};
  if (isObject(raw.sessions)) {
    Object.entries(raw.sessions).forEach(([key, value]) => {
      const session = sanitizeSession(value);
      if (session && knownDeviceIds.has(key)) {
        sessions[key] = session;
      }
    });
  }

  const deviceSnapshots: Record<string, DeviceDataCacheSnapshot> = {};
  if (isObject(raw.deviceSnapshots)) {
    Object.entries(raw.deviceSnapshots).forEach(([key, value]) => {
      const snapshot = sanitizeDeviceSnapshot(value);
      if (snapshot && knownDeviceIds.has(key)) {
        deviceSnapshots[key] = snapshot;
      }
    });
  }

  const selectedDeviceId =
    typeof raw.selectedDeviceId === "string" &&
    devices.some((device) => device.id === raw.selectedDeviceId)
      ? raw.selectedDeviceId
      : null;

  return {
    devices,
    selectedDeviceId,
    sessions,
    deviceSnapshots,
    sectionVisibility: sanitizeDeviceCardSectionState(raw.sectionVisibility),
  };
}

export async function loadExtensionState(): Promise<ExtensionState> {
  const stored = await browser.storage.local.get(STORAGE_KEY);
  return sanitizeExtensionState(stored[STORAGE_KEY]);
}

export async function saveExtensionState(state: ExtensionState): Promise<void> {
  await browser.storage.local.set({
    [STORAGE_KEY]: state,
  });
}
