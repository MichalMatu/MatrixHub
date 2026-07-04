import type { ExtensionState } from "$lib/infrastructure/chrome/storage";
import {
  buildCachedDataOrigins,
  createEmptySelectedDeviceDataOrigins,
  hasAnyCachedDeviceData,
  type SelectedDeviceDataOrigins,
} from "$lib/features/sidepanel/state/deviceDataCache";
import type { SidepanelState } from "$lib/features/sidepanel/state/sidepanelState";
import { createEmptyOverviewState } from "$lib/features/overview/state/overviewState";

interface SidepanelControllerStoreOptions {
  initialState: SidepanelState;
  onStateChange: (state: SidepanelState) => void;
  onError: (error: unknown) => void;
  saveExtensionState: (state: ExtensionState) => Promise<void>;
  onTransition?: (transition: SidepanelControllerTransition) => void;
}

export type SidepanelControllerPersistMode = "none" | "await" | "background";

export interface SidepanelControllerTransition {
  label: string;
  persist: SidepanelControllerPersistMode;
  selectedDeviceId: string | null;
  activeScreen: SidepanelState["activeScreen"];
  realtimeState: SidepanelState["realtimeState"];
}

interface SidepanelControllerCommitOptions {
  persist?: SidepanelControllerPersistMode;
}

export interface SidepanelControllerStore {
  getState(): SidepanelState;
  notify(label?: string, persist?: SidepanelControllerPersistMode): void;
  commit(
    label: string,
    mutate: (state: SidepanelState) => void,
    options?: SidepanelControllerCommitOptions,
  ): Promise<void>;
  persistState(): Promise<void>;
  persistInBackground(task: Promise<void>): void;
  removeDeviceSnapshot(deviceId: string): void;
  syncDeviceSnapshot(
    deviceId: string,
    nextOrigins?: Partial<SelectedDeviceDataOrigins>,
  ): void;
  syncSelectedDeviceSnapshot(
    nextOrigins?: Partial<SelectedDeviceDataOrigins>,
  ): void;
  restoreSelectedDeviceSnapshot(deviceId: string | null): void;
  resetSelectionState(): void;
}

export function createSidepanelControllerStore(
  options: SidepanelControllerStoreOptions,
): SidepanelControllerStore {
  const currentState = options.initialState;
  let persistenceQueue = Promise.resolve();

  function emitTransition(
    label: string,
    persist: SidepanelControllerPersistMode,
  ) {
    options.onTransition?.({
      label,
      persist,
      selectedDeviceId: currentState.selectedDeviceId,
      activeScreen: currentState.activeScreen,
      realtimeState: currentState.realtimeState,
    });
  }

  function notify(
    label = "state.notify",
    persist: SidepanelControllerPersistMode = "none",
  ) {
    options.onStateChange(currentState);
    emitTransition(label, persist);
  }

  function persistState() {
    const nextState: ExtensionState = {
      devices: currentState.devices,
      selectedDeviceId: currentState.selectedDeviceId,
      sessions: currentState.sessions,
      deviceSnapshots: currentState.deviceSnapshots,
      sectionVisibility: currentState.sectionVisibility,
    };

    const saveTask = persistenceQueue.then(() =>
      options.saveExtensionState(nextState),
    );
    persistenceQueue = saveTask.catch(() => undefined);
    return saveTask;
  }

  function persistInBackground(task: Promise<void>) {
    void task.catch((error) => {
      options.onError(error);
    });
  }

  function commit(
    label: string,
    mutate: (state: SidepanelState) => void,
    commitOptions: SidepanelControllerCommitOptions = {},
  ) {
    const persist = commitOptions.persist ?? "none";
    mutate(currentState);
    notify(label, persist);

    if (persist === "await") {
      return persistState();
    }

    if (persist === "background") {
      persistInBackground(persistState());
    }

    return Promise.resolve();
  }

  function updateSelectedDeviceDataOrigins(
    nextOrigins: Partial<SelectedDeviceDataOrigins>,
  ) {
    currentState.selectedDeviceDataOrigins = {
      ...currentState.selectedDeviceDataOrigins,
      ...nextOrigins,
    };
  }

  function removeDeviceSnapshot(deviceId: string) {
    if (!(deviceId in currentState.deviceSnapshots)) {
      return;
    }

    const { [deviceId]: _removed, ...nextSnapshots } =
      currentState.deviceSnapshots;
    currentState.deviceSnapshots = nextSnapshots;
  }

  function syncDeviceSnapshot(
    deviceId: string,
    nextOrigins: Partial<SelectedDeviceDataOrigins> = {},
  ) {
    const snapshot = {
      overviewState: currentState.overviewState,
      bleStatus: currentState.bleStatus,
      shellyDevices: currentState.shellyDevices,
      matrixSettings: currentState.matrixSettings,
    };

    if (hasAnyCachedDeviceData(snapshot)) {
      currentState.deviceSnapshots = {
        ...currentState.deviceSnapshots,
        [deviceId]: snapshot,
      };
    } else {
      removeDeviceSnapshot(deviceId);
    }

    if (Object.keys(nextOrigins).length > 0) {
      updateSelectedDeviceDataOrigins(nextOrigins);
    }
  }

  function syncSelectedDeviceSnapshot(
    nextOrigins: Partial<SelectedDeviceDataOrigins> = {},
  ) {
    if (!currentState.selectedDeviceId) {
      return;
    }

    syncDeviceSnapshot(currentState.selectedDeviceId, nextOrigins);
  }

  function restoreSelectedDeviceSnapshot(deviceId: string | null) {
    if (!deviceId) {
      currentState.overviewState = createEmptyOverviewState();
      currentState.bleStatus = null;
      currentState.shellyDevices = [];
      currentState.matrixSettings = null;
      currentState.matrixSettingsError = null;
      currentState.activity.isLoadingMatrixSettings = false;
      currentState.activity.isSavingMatrixSettings = false;
      currentState.activity.pendingShellyDeviceId = null;
      currentState.selectedDeviceDataOrigins =
        createEmptySelectedDeviceDataOrigins();
      currentState.realtimeState = "idle";
      return;
    }

    const snapshot = currentState.deviceSnapshots[deviceId] ?? null;
    currentState.overviewState =
      snapshot?.overviewState ?? createEmptyOverviewState();
    currentState.bleStatus = snapshot?.bleStatus ?? null;
    currentState.shellyDevices = snapshot?.shellyDevices ?? [];
    currentState.matrixSettings = snapshot?.matrixSettings ?? null;
    currentState.matrixSettingsError = null;
    currentState.activity.isLoadingMatrixSettings = false;
    currentState.activity.isSavingMatrixSettings = false;
    currentState.activity.pendingShellyDeviceId = null;
    currentState.selectedDeviceDataOrigins = buildCachedDataOrigins(snapshot);
    currentState.realtimeState = "idle";
  }

  function resetSelectionState() {
    restoreSelectedDeviceSnapshot(null);
  }

  return {
    getState: () => currentState,
    notify,
    commit,
    persistState,
    persistInBackground,
    removeDeviceSnapshot,
    syncDeviceSnapshot,
    syncSelectedDeviceSnapshot,
    restoreSelectedDeviceSnapshot,
    resetSelectionState,
  };
}
