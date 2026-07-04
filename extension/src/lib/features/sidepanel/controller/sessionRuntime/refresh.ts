import { applyBleStatusSnapshot } from "$lib/features/bluetooth/state/bluetoothState";
import { describeAppError } from "$lib/domain/shared/appError";
import { applyHttpOverviewSnapshot } from "$lib/features/overview/state/overviewState";
import {
  applyShellyOptimisticState,
  applyShellySnapshot,
} from "$lib/features/shelly/state/shellyState";
import { t } from "$lib/i18n/runtime";
import type {
  DeviceRecord,
  DeviceSession,
  MatrixSettings,
} from "@matrixhub/device-sdk";
import type {
  MatrixSettingsSaveOptions,
  SidepanelControllerDeps,
} from "../sidepanelControllerTypes";
import type { SidepanelControllerStore } from "../sidepanelControllerStore";
import type { SessionContextRuntime } from "./context";

interface SessionRefreshRuntimeOptions {
  deps: SidepanelControllerDeps;
  store: SidepanelControllerStore;
  sessionContext: SessionContextRuntime;
  onInfo: (message: string) => void;
  onError: (error: unknown) => void;
  clearMessages: () => void;
}

export interface SessionRefreshRuntime {
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
  refreshSupplementalDataFor(
    device: DeviceRecord,
    session: DeviceSession,
  ): Promise<void>;
}

export function createSessionRefreshRuntime(
  options: SessionRefreshRuntimeOptions,
): SessionRefreshRuntime {
  const { deps, store, sessionContext } = options;
  let matrixSettingsStateRequestId = 0;
  let matrixSettingsLoadRequestId = 0;

  async function refreshOverviewFor(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    const api = sessionContext.createSystemApiForDevice(device, session);
    if (!api) {
      return;
    }

    const { connectedAt, systemInfo } = await deps.fetchOverviewSnapshot(api);
    if (
      !sessionContext.isSessionContextActive(device.id, session.accessToken)
    ) {
      return;
    }

    await store.commit(
      "overview.refresh.applied",
      (currentState) => {
        currentState.overviewState = applyHttpOverviewSnapshot(
          currentState.overviewState,
          {
            connectedAt,
            systemInfo,
          },
        );
        currentState.devices = deps.markDeviceConnected(
          currentState.devices,
          device.id,
          connectedAt,
        );
        store.syncDeviceSnapshot(device.id, {
          overview: "live",
        });
      },
      {
        persist: "await",
      },
    );
  }

  async function refreshBluetoothFor(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    const api = sessionContext.createBleApiForDevice(device, session);
    if (!api) {
      return;
    }

    const status = await api.getStatus();
    if (
      !sessionContext.isSessionContextActive(device.id, session.accessToken)
    ) {
      return;
    }

    await store.commit(
      "bluetooth.refresh.applied",
      (currentState) => {
        currentState.bleStatus = applyBleStatusSnapshot(
          currentState.bleStatus,
          status,
        );
        store.syncDeviceSnapshot(device.id, {
          ble: "live",
        });
      },
      {
        persist: "await",
      },
    );
  }

  async function refreshShellyFor(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    const api = sessionContext.createShellyApiForDevice(device, session);
    if (!api) {
      return;
    }

    const shellyDevices = await api.getDevices();
    if (
      !sessionContext.isSessionContextActive(device.id, session.accessToken)
    ) {
      return;
    }

    await store.commit(
      "shelly.refresh.applied",
      (currentState) => {
        currentState.shellyDevices = applyShellySnapshot(shellyDevices);
        store.syncDeviceSnapshot(device.id, {
          shelly: "live",
        });
      },
      {
        persist: "await",
      },
    );
  }

  async function refreshMatrixSettingsFor(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    const api = sessionContext.createMatrixApiForDevice(device, session);
    if (!api) {
      return;
    }

    const stateRequestId = ++matrixSettingsStateRequestId;
    const loadRequestId = ++matrixSettingsLoadRequestId;

    if (sessionContext.isSessionContextActive(device.id, session.accessToken)) {
      await store.commit("matrix.refresh.started", (currentState) => {
        currentState.activity.isLoadingMatrixSettings = true;
        currentState.matrixSettingsError = null;
      });
    }

    try {
      const matrixSettings = await api.getSettings();
      if (
        !sessionContext.isSessionContextActive(
          device.id,
          session.accessToken,
        ) ||
        stateRequestId !== matrixSettingsStateRequestId
      ) {
        return;
      }

      await store.commit(
        "matrix.refresh.applied",
        (currentState) => {
          currentState.matrixSettings = matrixSettings;
          currentState.matrixSettingsError = null;
          store.syncDeviceSnapshot(device.id, {
            matrix: "live",
          });
        },
        {
          persist: "await",
        },
      );
    } catch (error) {
      if (
        !sessionContext.isSessionContextActive(
          device.id,
          session.accessToken,
        ) ||
        stateRequestId !== matrixSettingsStateRequestId
      ) {
        return;
      }

      await store.commit("matrix.refresh.failed", (currentState) => {
        currentState.matrixSettingsError = describeAppError(error);
      });
      throw error;
    } finally {
      if (
        sessionContext.isSessionContextActive(device.id, session.accessToken) &&
        loadRequestId === matrixSettingsLoadRequestId
      ) {
        await store.commit("matrix.refresh.finished", (currentState) => {
          currentState.activity.isLoadingMatrixSettings = false;
        });
      }
    }
  }

  async function saveMatrixSettingsFor(
    device: DeviceRecord,
    session: DeviceSession,
    settings: Partial<MatrixSettings>,
    saveOptions: MatrixSettingsSaveOptions = {},
  ) {
    const api = sessionContext.createMatrixApiForDevice(device, session);
    if (!api) {
      return;
    }

    const stateRequestId = ++matrixSettingsStateRequestId;
    await store.commit("matrix.save.started", (currentState) => {
      currentState.activity.isSavingMatrixSettings = true;
      currentState.matrixSettingsError = null;
    });

    try {
      const matrixSettings = await api.updateSettings(settings);
      if (
        !sessionContext.isSessionContextActive(
          device.id,
          session.accessToken,
        ) ||
        stateRequestId !== matrixSettingsStateRequestId
      ) {
        return;
      }

      await store.commit(
        "matrix.save.applied",
        (currentState) => {
          currentState.matrixSettings = matrixSettings;
          currentState.matrixSettingsError = null;
          store.syncDeviceSnapshot(device.id, {
            matrix: "live",
          });
        },
        {
          persist: "await",
        },
      );

      if (saveOptions.notify !== false) {
        options.onInfo(t("controller.matrixSettingsSaved"));
      }
    } catch (error) {
      if (
        !sessionContext.isSessionContextActive(
          device.id,
          session.accessToken,
        ) ||
        stateRequestId !== matrixSettingsStateRequestId
      ) {
        return;
      }

      await store.commit("matrix.save.failed", (currentState) => {
        currentState.matrixSettingsError = describeAppError(error);
      });
      throw error;
    } finally {
      if (
        sessionContext.isSessionContextActive(device.id, session.accessToken)
      ) {
        await store.commit("matrix.save.finished", (currentState) => {
          currentState.activity.isSavingMatrixSettings = false;
        });
      }
    }
  }

  async function toggleShellyFor(
    device: DeviceRecord,
    session: DeviceSession,
    shellyDeviceId: string,
    turnOn: boolean,
  ) {
    const api = sessionContext.createShellyApiForDevice(device, session);
    if (!api) {
      return;
    }

    await store.commit("shelly.toggle.started", (currentState) => {
      currentState.activity.pendingShellyDeviceId = shellyDeviceId;
    });

    try {
      await api.setRelayState(shellyDeviceId, turnOn);
      if (
        !sessionContext.isSessionContextActive(device.id, session.accessToken)
      ) {
        return;
      }

      await store.commit(
        "shelly.toggle.applied",
        (currentState) => {
          currentState.shellyDevices = applyShellyOptimisticState(
            currentState.shellyDevices,
            shellyDeviceId,
            turnOn,
          );
          store.syncDeviceSnapshot(device.id, {
            shelly: "live",
          });
        },
        {
          persist: "background",
        },
      );
      options.onInfo(t("controller.shellyCommandQueued"));
    } finally {
      if (
        sessionContext.isSessionContextActive(device.id, session.accessToken)
      ) {
        await store.commit("shelly.toggle.finished", (currentState) => {
          currentState.activity.pendingShellyDeviceId = null;
        });
      }
    }
  }

  async function triggerWifiRecoveryFor(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    const api = sessionContext.createSystemApiForDevice(device, session);
    if (!api) {
      return;
    }

    options.clearMessages();
    const message = await deps.requestWifiRecoveryMessage(api);
    if (
      !sessionContext.isSessionContextActive(device.id, session.accessToken)
    ) {
      return;
    }

    options.onInfo(message);
  }

  async function refreshSupplementalDataFor(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    try {
      await refreshBluetoothFor(device, session);
    } catch (error) {
      options.onError(error);
    }

    try {
      await refreshShellyFor(device, session);
    } catch (error) {
      options.onError(error);
    }

    try {
      await refreshMatrixSettingsFor(device, session);
    } catch (error) {
      options.onError(error);
    }
  }

  return {
    refreshOverviewFor,
    refreshMatrixSettingsFor,
    saveMatrixSettingsFor,
    toggleShellyFor,
    triggerWifiRecoveryFor,
    refreshSupplementalDataFor,
  };
}
