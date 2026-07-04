import {
  type SensorTelemetryEvent,
  type SystemInformation,
  type SystemStatus,
  type SystemStatusSnapshot,
  type TelemetrySnapshot,
} from "@matrixhub/device-sdk";
import {
  appendTelemetryHistoryPoint,
  normalizeTelemetrySnapshot,
} from "$lib/features/overview/telemetryHistory";

export interface OverviewState {
  telemetrySnapshot: TelemetrySnapshot | null;
  systemInfo: SystemInformation | null;
  systemStatusSnapshot: SystemStatusSnapshot | null;
  systemStatus: SystemStatus | null;
  lastSuccessfulRefreshAt: string | null;
  lastTelemetryAt: string | null;
}

const MIN_VALID_ABSOLUTE_TIMESTAMP_MS = Date.UTC(2000, 0, 1);

export function createEmptyOverviewState(): OverviewState {
  return {
    telemetrySnapshot: null,
    systemInfo: null,
    systemStatusSnapshot: null,
    systemStatus: null,
    lastSuccessfulRefreshAt: null,
    lastTelemetryAt: null,
  };
}

export function applySensorEventToSnapshot(
  snapshot: TelemetrySnapshot | null,
  event: SensorTelemetryEvent,
  now = Date.now(),
): TelemetrySnapshot {
  return {
    co2: event.co2,
    temp: event.temp,
    humid: event.humid,
    lastReadOk: event.lastReadOk,
    history: appendTelemetryHistoryPoint(
      snapshot?.history,
      {
        co2: event.co2,
        temp: event.temp,
        humid: event.humid,
      },
      {
        timestampMs: event.timestamp_ms,
        fallbackNow: now,
      },
    ),
  };
}

function toIsoString(
  timestampMs: number | null | undefined,
  fallbackNow = Date.now(),
) {
  // Realtime telemetry currently sends sensor timestamps as device uptime
  // millis(), not wall-clock epoch millis. Treat implausibly old values as
  // relative/invalid and anchor them to the moment the event was received.
  if (
    typeof timestampMs === "number" &&
    Number.isFinite(timestampMs) &&
    timestampMs >= MIN_VALID_ABSOLUTE_TIMESTAMP_MS
  ) {
    return new Date(timestampMs).toISOString();
  }

  return new Date(fallbackNow).toISOString();
}

export function applyHttpOverviewSnapshot(
  state: OverviewState,
  input: {
    connectedAt: string;
    systemInfo: SystemInformation;
  },
): OverviewState {
  return {
    ...state,
    systemInfo: input.systemInfo,
    lastSuccessfulRefreshAt: input.connectedAt,
  };
}

export function applyRealtimeTelemetrySnapshot(
  state: OverviewState,
  snapshot: TelemetrySnapshot,
  receivedAt = new Date().toISOString(),
): OverviewState {
  return {
    ...state,
    telemetrySnapshot: normalizeTelemetrySnapshot(snapshot),
    lastTelemetryAt: receivedAt,
  };
}

export function applyRealtimeSystemSnapshot(
  state: OverviewState,
  snapshot: SystemStatusSnapshot,
  receivedAt = new Date().toISOString(),
): OverviewState {
  return {
    ...state,
    systemInfo: snapshot.system_info ?? state.systemInfo,
    systemStatusSnapshot: snapshot,
    lastSuccessfulRefreshAt: receivedAt,
  };
}

export function applyRealtimeTelemetryEvent(
  state: OverviewState,
  event: SensorTelemetryEvent,
  now = Date.now(),
): OverviewState {
  return {
    ...state,
    telemetrySnapshot: applySensorEventToSnapshot(
      state.telemetrySnapshot,
      event,
      now,
    ),
    lastTelemetryAt: toIsoString(event.timestamp_ms, now),
  };
}

export function applySystemStatusPacket(
  state: OverviewState,
  systemStatus: SystemStatus,
): OverviewState {
  return {
    ...state,
    systemStatus,
  };
}
