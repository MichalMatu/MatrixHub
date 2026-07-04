import type {
  BleApiLike,
  MatrixApiLike,
  OverviewSocketLike,
  SelectionApiFactory,
  ShellyApiLike,
  SidepanelControllerDeps,
  SystemApiLike,
} from "../sidepanelControllerTypes";
import type { SidepanelControllerStore } from "../sidepanelControllerStore";
import {
  getCurrentSession,
  getSelectedDevice,
} from "$lib/features/sidepanel/state/sidepanelState";
import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
import { t } from "$lib/i18n/runtime";

interface SessionContextRuntimeOptions {
  deps: SidepanelControllerDeps;
  store: SidepanelControllerStore;
  overviewSocket: OverviewSocketLike;
  logout: (message?: string) => Promise<void>;
}

export interface SessionContextRuntime {
  getSelection(): {
    selectedDevice: DeviceRecord | null;
    currentSession: DeviceSession | null;
  };
  isSessionContextActive(deviceId: string, accessToken: string): boolean;
  hydrateRealtimeAuthForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): Promise<void>;
  connectRealtimeForDevice(device: DeviceRecord, session: DeviceSession): void;
  createSystemApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): SystemApiLike | null;
  createBleApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): BleApiLike | null;
  createShellyApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): ShellyApiLike | null;
  createMatrixApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): MatrixApiLike | null;
}

export function createSessionContextRuntime(
  options: SessionContextRuntimeOptions,
): SessionContextRuntime {
  const { deps, store } = options;

  function getSelection() {
    const currentState = store.getState();
    return {
      selectedDevice: getSelectedDevice(currentState),
      currentSession: getCurrentSession(currentState),
    };
  }

  function isSessionContextActive(deviceId: string, accessToken: string) {
    const currentState = store.getState();
    return (
      currentState.selectedDeviceId === deviceId &&
      currentState.sessions[deviceId]?.accessToken === accessToken
    );
  }

  async function hydrateRealtimeAuthForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    if (!isSessionContextActive(device.id, session.accessToken)) {
      return;
    }

    await deps.syncWsAccessTokenCookie(device.origin, session.accessToken);
  }

  function connectRealtimeForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    if (!isSessionContextActive(device.id, session.accessToken)) {
      return;
    }

    options.overviewSocket.connect(device.origin);
  }

  async function handleUnauthorized(
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    if (!isSessionContextActive(device.id, session.accessToken)) {
      return;
    }

    await options.logout(t("controller.sessionExpired"));
  }

  function createApiForDevice<TApi>(
    factory: SelectionApiFactory<TApi>,
    device: DeviceRecord,
    session: DeviceSession,
  ) {
    return factory({
      selectedDevice: device,
      currentSession: session,
      onUnauthorized: () => {
        void handleUnauthorized(device, session);
      },
    });
  }

  function createSystemApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): SystemApiLike | null {
    return createApiForDevice(
      deps.createSystemApiForSelection,
      device,
      session,
    );
  }

  function createBleApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): BleApiLike | null {
    return createApiForDevice(deps.createBleApiForSelection, device, session);
  }

  function createShellyApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): ShellyApiLike | null {
    return createApiForDevice(
      deps.createShellyApiForSelection,
      device,
      session,
    );
  }

  function createMatrixApiForDevice(
    device: DeviceRecord,
    session: DeviceSession,
  ): MatrixApiLike | null {
    return createApiForDevice(
      deps.createMatrixApiForSelection,
      device,
      session,
    );
  }

  return {
    getSelection,
    isSessionContextActive,
    hydrateRealtimeAuthForDevice,
    connectRealtimeForDevice,
    createSystemApiForDevice,
    createBleApiForDevice,
    createShellyApiForDevice,
    createMatrixApiForDevice,
  };
}
