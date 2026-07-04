import type {
  BleStatus,
  DeviceRecord,
  DeviceSession,
  MatrixSettings,
  ShellyDevice,
} from "@matrixhub/device-sdk";
import type { CredentialErrors } from "$lib/features/auth/validation/credentials";
import { createCredentialErrors } from "$lib/features/auth/validation/credentials";
import type { DeviceDraftErrors } from "$lib/features/devices/validation/deviceDraft";
import { createDeviceDraftErrors } from "$lib/features/devices/validation/deviceDraft";
import type { RealtimeConnectionState } from "$lib/features/realtime/socket/deviceOverviewSocket";
import type { OverviewState } from "$lib/features/overview/state/overviewState";
import { createEmptyOverviewState } from "$lib/features/overview/state/overviewState";
import {
  createEmptySelectedDeviceDataOrigins,
  type DeviceDataCacheSnapshot,
  type SelectedDeviceDataOrigins,
} from "$lib/features/sidepanel/state/deviceDataCache";
import {
  createDefaultDeviceCardSectionState,
  type DeviceCardSectionState,
} from "$lib/features/sidepanel/state/deviceCardSections";
import {
  readStoredSidepanelTheme,
  type SidepanelThemeName,
} from "$lib/features/theme/themePresets";

export type SidepanelScreen = "devices" | "settings" | "charts";

export interface DeviceDraftState {
  name: string;
  address: string;
}

export interface CredentialsState {
  username: string;
  password: string;
}

export interface SidepanelActivityState {
  isSavingDevice: boolean;
  isSigningIn: boolean;
  isLoadingMatrixSettings: boolean;
  isSavingMatrixSettings: boolean;
  pendingShellyDeviceId: string | null;
}

export interface SidepanelState {
  devices: DeviceRecord[];
  selectedDeviceId: string | null;
  sessions: Record<string, DeviceSession>;
  deviceDraft: DeviceDraftState;
  credentials: CredentialsState;
  deviceFormErrors: DeviceDraftErrors;
  authFormErrors: CredentialErrors;
  isBootstrapping: boolean;
  activity: SidepanelActivityState;
  activeScreen: SidepanelScreen;
  realtimeState: RealtimeConnectionState;
  activeTheme: SidepanelThemeName;
  isThemePaletteOpen: boolean;
  isAddSheetOpen: boolean;
  deviceSnapshots: Record<string, DeviceDataCacheSnapshot>;
  sectionVisibility: DeviceCardSectionState;
  selectedDeviceDataOrigins: SelectedDeviceDataOrigins;
  overviewState: OverviewState;
  bleStatus: BleStatus | null;
  shellyDevices: ShellyDevice[];
  matrixSettings: MatrixSettings | null;
  matrixSettingsError: string | null;
}

export function createInitialSidepanelState(
  activeTheme: SidepanelThemeName = readStoredSidepanelTheme(),
): SidepanelState {
  return {
    devices: [],
    selectedDeviceId: null,
    sessions: {},
    deviceDraft: {
      name: "",
      address: "",
    },
    credentials: {
      username: "",
      password: "",
    },
    deviceFormErrors: createDeviceDraftErrors(),
    authFormErrors: createCredentialErrors(),
    isBootstrapping: true,
    activity: {
      isSavingDevice: false,
      isSigningIn: false,
      isLoadingMatrixSettings: false,
      isSavingMatrixSettings: false,
      pendingShellyDeviceId: null,
    },
    activeScreen: "devices",
    realtimeState: "idle",
    activeTheme,
    isThemePaletteOpen: false,
    isAddSheetOpen: false,
    deviceSnapshots: {},
    sectionVisibility: createDefaultDeviceCardSectionState(),
    selectedDeviceDataOrigins: createEmptySelectedDeviceDataOrigins(),
    overviewState: createEmptyOverviewState(),
    bleStatus: null,
    shellyDevices: [],
    matrixSettings: null,
    matrixSettingsError: null,
  };
}

export function getSelectedDevice(state: SidepanelState): DeviceRecord | null {
  return (
    state.devices.find((device) => device.id === state.selectedDeviceId) ?? null
  );
}

export function getCurrentSession(state: SidepanelState): DeviceSession | null {
  const device = getSelectedDevice(state);
  return device ? (state.sessions[device.id] ?? null) : null;
}
