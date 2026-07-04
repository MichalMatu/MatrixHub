import { createCredentialErrors } from "$lib/features/auth/validation/credentials";
import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
import type { SidepanelControllerDeps } from "../sidepanelControllerTypes";
import type { SidepanelControllerStore } from "../sidepanelControllerStore";
import type { OverviewSocketLike } from "../sidepanelControllerTypes";
import type { SessionContextRuntime } from "./context";
import type { SessionRefreshRuntime } from "./refresh";

interface SessionLifecycleRuntimeOptions {
  deps: SidepanelControllerDeps;
  store: SidepanelControllerStore;
  overviewSocket: OverviewSocketLike;
  sessionContext: SessionContextRuntime;
  refreshRuntime: SessionRefreshRuntime;
  onError: (error: unknown) => void;
  clearMessages: () => void;
}

export interface SessionLifecycleRuntime {
  completeSignIn(
    device: DeviceRecord,
    session: DeviceSession,
    clearPassword: () => void,
  ): Promise<void>;
  restoreSelectedSessionIfAvailable(): Promise<boolean>;
  applySelectedDeviceChange(
    nextSelectedDeviceId: string | null,
    optionsOverride?: { persist?: boolean },
  ): Promise<boolean>;
}

export function createSessionLifecycleRuntime(
  options: SessionLifecycleRuntimeOptions,
): SessionLifecycleRuntime {
  const { deps, store, sessionContext, refreshRuntime } = options;

  async function restoreSession(device: DeviceRecord, session: DeviceSession) {
    try {
      await deps.restoreSelectedSessionFlow({
        selectedDevice: device,
        currentSession: session,
        resetOverviewState: () => {
          void store.commit("session.restore.cacheHydrated", () => {
            store.restoreSelectedDeviceSnapshot(device.id);
          });
        },
        hydrateRealtimeAuth: () =>
          sessionContext.hydrateRealtimeAuthForDevice(device, session),
        refreshOverview: () =>
          refreshRuntime.refreshOverviewFor(device, session),
        connectRealtime: () =>
          sessionContext.connectRealtimeForDevice(device, session),
        onRefreshOverviewError: options.onError,
      });

      await refreshRuntime.refreshSupplementalDataFor(device, session);
    } catch (error) {
      options.onError(error);
    }
  }

  async function restoreSelectedSessionIfAvailable() {
    await deps.afterSessionApplied();

    const { selectedDevice, currentSession } = sessionContext.getSelection();
    if (!selectedDevice || !currentSession) {
      return false;
    }

    await restoreSession(selectedDevice, currentSession);
    return true;
  }

  async function applySelectedDeviceChange(
    nextSelectedDeviceId: string | null,
    optionsOverride: { persist?: boolean } = {},
  ) {
    const currentState = store.getState();
    if (nextSelectedDeviceId === currentState.selectedDeviceId) {
      return false;
    }

    options.overviewSocket.disconnect();
    options.clearMessages();
    await store.commit(
      "selection.changed",
      (state) => {
        store.resetSelectionState();
        state.authFormErrors = createCredentialErrors();
        state.selectedDeviceId = nextSelectedDeviceId;
        store.restoreSelectedDeviceSnapshot(nextSelectedDeviceId);
      },
      {
        persist: (optionsOverride.persist ?? true) ? "await" : "none",
      },
    );

    await restoreSelectedSessionIfAvailable();
    return true;
  }

  async function completeSignIn(
    device: DeviceRecord,
    session: DeviceSession,
    clearPassword: () => void,
  ) {
    await deps.completeSignInFlow({
      afterSessionApplied: deps.afterSessionApplied,
      hydrateRealtimeAuth: () =>
        sessionContext.hydrateRealtimeAuthForDevice(device, session),
      persistState: store.persistState,
      refreshOverview: () => refreshRuntime.refreshOverviewFor(device, session),
      connectRealtime: () =>
        sessionContext.connectRealtimeForDevice(device, session),
      clearPassword: () => {
        clearPassword();
        store.notify("auth.signIn.passwordCleared");
      },
      onRefreshOverviewError: options.onError,
    });

    await refreshRuntime.refreshSupplementalDataFor(device, session);
  }

  return {
    completeSignIn,
    restoreSelectedSessionIfAvailable,
    applySelectedDeviceChange,
  };
}
