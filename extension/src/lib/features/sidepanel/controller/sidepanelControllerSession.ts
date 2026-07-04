import type {
  DeviceRecord,
  DeviceSession,
  MatrixSettings,
} from "@matrixhub/device-sdk";
import type { SidepanelControllerStore } from "./sidepanelControllerStore";
import type {
  MatrixSettingsSaveOptions,
  OverviewSocketLike,
  SidepanelControllerDeps,
} from "./sidepanelControllerTypes";
import { createSessionContextRuntime } from "./sessionRuntime/context";
import { createSessionLifecycleRuntime } from "./sessionRuntime/lifecycle";
import { createSessionRefreshRuntime } from "./sessionRuntime/refresh";

interface SidepanelControllerSessionRuntimeOptions {
  deps: SidepanelControllerDeps;
  store: SidepanelControllerStore;
  overviewSocket: OverviewSocketLike;
  onInfo: (message: string) => void;
  onError: (error: unknown) => void;
  clearMessages: () => void;
  logout: (message?: string) => Promise<void>;
}

export interface SidepanelControllerSessionRuntime {
  getSelection(): {
    selectedDevice: DeviceRecord | null;
    currentSession: DeviceSession | null;
  };
  completeSignIn(
    device: DeviceRecord,
    session: DeviceSession,
    clearPassword: () => void,
  ): Promise<void>;
  refreshOverviewFor(
    device: DeviceRecord,
    session: DeviceSession,
  ): Promise<void>;
  refreshMatrixSettingsFor(
    device: DeviceRecord,
    session: DeviceSession,
  ): Promise<void>;
  saveMatrixSettingsFor(
    device: DeviceRecord,
    session: DeviceSession,
    settings: Partial<MatrixSettings>,
    options?: MatrixSettingsSaveOptions,
  ): Promise<void>;
  toggleShellyFor(
    device: DeviceRecord,
    session: DeviceSession,
    shellyDeviceId: string,
    turnOn: boolean,
  ): Promise<void>;
  triggerWifiRecoveryFor(
    device: DeviceRecord,
    session: DeviceSession,
  ): Promise<void>;
  restoreSelectedSessionIfAvailable(): Promise<boolean>;
  applySelectedDeviceChange(
    nextSelectedDeviceId: string | null,
    optionsOverride?: { persist?: boolean },
  ): Promise<boolean>;
}

export function createSidepanelControllerSessionRuntime(
  options: SidepanelControllerSessionRuntimeOptions,
): SidepanelControllerSessionRuntime {
  const sessionContext = createSessionContextRuntime({
    deps: options.deps,
    store: options.store,
    overviewSocket: options.overviewSocket,
    logout: options.logout,
  });
  const refreshRuntime = createSessionRefreshRuntime({
    deps: options.deps,
    store: options.store,
    sessionContext,
    onInfo: options.onInfo,
    onError: options.onError,
    clearMessages: options.clearMessages,
  });
  const lifecycleRuntime = createSessionLifecycleRuntime({
    deps: options.deps,
    store: options.store,
    overviewSocket: options.overviewSocket,
    sessionContext,
    refreshRuntime,
    onError: options.onError,
    clearMessages: options.clearMessages,
  });

  return {
    getSelection: sessionContext.getSelection,
    completeSignIn: lifecycleRuntime.completeSignIn,
    refreshOverviewFor: refreshRuntime.refreshOverviewFor,
    refreshMatrixSettingsFor: refreshRuntime.refreshMatrixSettingsFor,
    saveMatrixSettingsFor: refreshRuntime.saveMatrixSettingsFor,
    toggleShellyFor: refreshRuntime.toggleShellyFor,
    triggerWifiRecoveryFor: refreshRuntime.triggerWifiRecoveryFor,
    restoreSelectedSessionIfAvailable:
      lifecycleRuntime.restoreSelectedSessionIfAvailable,
    applySelectedDeviceChange: lifecycleRuntime.applySelectedDeviceChange,
  };
}
