import {
  applyBleDeviceEvent,
  applyBleStatusSnapshot,
} from "$lib/features/bluetooth/state/bluetoothState";
import {
  applyRealtimeSystemSnapshot,
  applyRealtimeTelemetryEvent,
  applyRealtimeTelemetrySnapshot,
  applySystemStatusPacket,
} from "$lib/features/overview/state/overviewState";
import {
  applyShellyEvent,
  applyShellySnapshot,
  resolveShellyEventDeviceId,
} from "$lib/features/shelly/state/shellyState";
import type { SidepanelControllerStore } from "./sidepanelControllerStore";
import type {
  OverviewSocketLike,
  SidepanelControllerDeps,
} from "./sidepanelControllerTypes";

interface SidepanelControllerRealtimeOptions {
  createOverviewSocket: SidepanelControllerDeps["createOverviewSocket"];
  store: SidepanelControllerStore;
  onError: (error: unknown) => void;
}

export function createSidepanelOverviewSocket(
  options: SidepanelControllerRealtimeOptions,
): OverviewSocketLike {
  const { store } = options;

  return options.createOverviewSocket({
    onConnectionStateChange: (realtimeState) => {
      void store.commit("realtime.connection.changed", (currentState) => {
        currentState.realtimeState = realtimeState;
      });
    },
    onTelemetrySnapshot: (snapshot) => {
      void store.commit(
        "realtime.telemetry.snapshot",
        (currentState) => {
          currentState.overviewState = applyRealtimeTelemetrySnapshot(
            currentState.overviewState,
            snapshot,
          );
          store.syncSelectedDeviceSnapshot({
            telemetry: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onSystemSnapshot: (snapshot) => {
      void store.commit(
        "realtime.system.snapshot",
        (currentState) => {
          currentState.overviewState = applyRealtimeSystemSnapshot(
            currentState.overviewState,
            snapshot,
          );
          store.syncSelectedDeviceSnapshot({
            overview: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onSystemPacket: (packet) => {
      void store.commit(
        "realtime.system.packet",
        (currentState) => {
          currentState.overviewState = applySystemStatusPacket(
            currentState.overviewState,
            packet,
          );
          store.syncSelectedDeviceSnapshot({
            overview: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onTelemetryEvent: (event) => {
      void store.commit(
        "realtime.telemetry.event",
        (currentState) => {
          currentState.overviewState = applyRealtimeTelemetryEvent(
            currentState.overviewState,
            event,
          );
          store.syncSelectedDeviceSnapshot({
            telemetry: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onBleSnapshot: (snapshot) => {
      void store.commit(
        "realtime.ble.snapshot",
        (currentState) => {
          currentState.bleStatus = applyBleStatusSnapshot(
            currentState.bleStatus,
            snapshot,
          );
          store.syncSelectedDeviceSnapshot({
            ble: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onBleEvent: (event) => {
      void store.commit(
        "realtime.ble.event",
        (currentState) => {
          currentState.bleStatus = applyBleDeviceEvent(
            currentState.bleStatus,
            event,
          );
          store.syncSelectedDeviceSnapshot({
            ble: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onShellySnapshot: (snapshot) => {
      void store.commit(
        "realtime.shelly.snapshot",
        (currentState) => {
          currentState.shellyDevices = applyShellySnapshot(snapshot);
          store.syncSelectedDeviceSnapshot({
            shelly: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onShellyEvent: (event) => {
      void store.commit(
        "realtime.shelly.event",
        (currentState) => {
          const matchedDeviceId = resolveShellyEventDeviceId(
            currentState.shellyDevices,
            event.id,
          );
          currentState.shellyDevices = applyShellyEvent(
            currentState.shellyDevices,
            event,
          );
          if (
            matchedDeviceId &&
            currentState.activity.pendingShellyDeviceId === matchedDeviceId
          ) {
            currentState.activity.pendingShellyDeviceId = null;
          }
          store.syncSelectedDeviceSnapshot({
            shelly: "live",
          });
        },
        {
          persist: "background",
        },
      );
    },
    onError: () => {
      options.onError(
        "Live updates paused. The device card will keep retrying in the background.",
      );
    },
  });
}
