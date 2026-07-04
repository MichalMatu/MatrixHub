import type { DeviceOverviewSocketHandlers } from "$lib/features/realtime/socket/deviceOverviewSocket";
import type {
  completeSignInFlow,
  restoreSelectedSessionFlow,
} from "$lib/features/auth/actions/sessionFlows";
import type { signInSelectedDevice } from "$lib/features/auth/actions/signIn";
import type { saveDeviceDraft } from "$lib/features/devices/actions/deviceDraft";
import type {
  buildRemoveDeviceContext,
  clearSelectedSessionContext,
} from "$lib/features/devices/actions/deviceSelection";
import type { markDeviceConnected } from "$lib/domain/device/rules";
import type { SidepanelState } from "$lib/features/sidepanel/state/sidepanelState";
import type { DeviceCardSectionKey } from "$lib/features/sidepanel/state/deviceCardSections";
import type { ExtensionState } from "$lib/infrastructure/chrome/storage";
import type {
  BleStatus,
  DeviceRecord,
  DeviceSession,
  MatrixSettings,
  ShellyDevice,
  SystemInformation,
  WifiRecoveryResponse,
} from "@matrixhub/device-sdk";
import type { SidepanelControllerTransition } from "./sidepanelControllerStore";

export interface SystemApiLike {
  getSystemInfo(): Promise<SystemInformation>;
  triggerWifiRecovery(): Promise<WifiRecoveryResponse>;
}

export interface BleApiLike {
  getStatus(): Promise<BleStatus>;
}

export interface ShellyApiLike {
  getDevices(): Promise<ShellyDevice[]>;
  setRelayState(id: string, on: boolean): Promise<void>;
}

export interface MatrixApiLike {
  getSettings(): Promise<MatrixSettings>;
  updateSettings(settings: Partial<MatrixSettings>): Promise<MatrixSettings>;
}

export interface OverviewSocketLike {
  connect(origin: string): void;
  disconnect(): void;
}

export interface DeviceSelectionApiContext {
  selectedDevice: DeviceRecord | null;
  currentSession: DeviceSession | null;
  onUnauthorized: () => void;
}

export type SelectionApiFactory<TApi> = (
  context: DeviceSelectionApiContext,
) => TApi | null;

export interface SidepanelControllerDeps {
  afterSessionApplied: () => Promise<void>;
  loadExtensionState: () => Promise<ExtensionState>;
  saveExtensionState: (state: ExtensionState) => Promise<void>;
  syncWsAccessTokenCookie: (
    origin: string,
    accessToken: string,
  ) => Promise<void>;
  clearWsAccessTokenCookie: (origin: string) => Promise<void>;
  saveDeviceDraft: typeof saveDeviceDraft;
  signInSelectedDevice: typeof signInSelectedDevice;
  restoreSelectedSessionFlow: typeof restoreSelectedSessionFlow;
  completeSignInFlow: typeof completeSignInFlow;
  createSystemApiForSelection: SelectionApiFactory<SystemApiLike>;
  createBleApiForSelection: SelectionApiFactory<BleApiLike>;
  createShellyApiForSelection: SelectionApiFactory<ShellyApiLike>;
  createMatrixApiForSelection: SelectionApiFactory<MatrixApiLike>;
  fetchOverviewSnapshot: (api: SystemApiLike) => Promise<{
    connectedAt: string;
    systemInfo: SystemInformation;
  }>;
  requestWifiRecoveryMessage: (api: SystemApiLike) => Promise<string>;
  buildRemoveDeviceContext: typeof buildRemoveDeviceContext;
  clearSelectedSessionContext: typeof clearSelectedSessionContext;
  markDeviceConnected: typeof markDeviceConnected;
  createOverviewSocket: (
    handlers: DeviceOverviewSocketHandlers,
  ) => OverviewSocketLike;
}

export interface SidepanelControllerOptions {
  state: SidepanelState;
  onStateChange: (state: SidepanelState) => void;
  onInfo: (message: string) => void;
  onError: (error: unknown) => void;
  clearMessages: () => void;
  onTransition?: (transition: SidepanelControllerTransition) => void;
  deps?: Partial<SidepanelControllerDeps>;
}

export interface MatrixSettingsSaveOptions {
  notify?: boolean;
}

export interface SidepanelController {
  bootstrap(): Promise<void>;
  addDevice(): Promise<void>;
  signIn(): Promise<void>;
  refreshOverview(): Promise<void>;
  refreshMatrixSettings(): Promise<void>;
  saveMatrixSettings(
    settings: Partial<MatrixSettings>,
    options?: MatrixSettingsSaveOptions,
  ): Promise<void>;
  toggleShelly(shellyDeviceId: string, turnOn: boolean): Promise<void>;
  triggerWifiRecovery(): Promise<void>;
  selectDevice(deviceId: string): Promise<void>;
  setSectionOpen(section: DeviceCardSectionKey, open: boolean): Promise<void>;
  reconnectDevice(deviceId: string): Promise<void>;
  renameDevice(deviceId: string, name: string): Promise<boolean>;
  removeDevice(deviceId: string): Promise<void>;
  logout(message?: string): Promise<void>;
  focusDevice(deviceId: string): Promise<void>;
  destroy(): void;
}
