import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
import {
  formatCompactElapsedAgo,
  formatRssiValue,
  formatUptime,
} from "$lib/i18n/formatters";
import { getI18nRuntime, type I18nRuntime } from "$lib/i18n/runtime";
import type { RealtimeConnectionState } from "$lib/features/realtime/socket/deviceOverviewSocket";
import type { OverviewState } from "$lib/features/overview/state/overviewState";
import type { DetailItem, DeviceSettingsViewModel } from "./viewModelTypes";

const MIN_VALID_TIMESTAMP_MS = Date.UTC(2000, 0, 1);

interface SensorStateCopy {
  kind:
    | "waiting_first_sample"
    | "waiting"
    | "cached"
    | "read_failed"
    | "sampled"
    | "stale"
    | "fresh";
  label: string;
}

function parseValidTimestamp(timestamp: string | null) {
  if (!timestamp) {
    return null;
  }

  const parsed = Date.parse(timestamp);
  if (Number.isNaN(parsed) || parsed < MIN_VALID_TIMESTAMP_MS) {
    return null;
  }

  return parsed;
}

function formatLastSeen(
  timestamp: string | null,
  i18n: I18nRuntime,
  now = Date.now(),
) {
  const parsed = parseValidTimestamp(timestamp);
  if (parsed === null) {
    return null;
  }

  return formatCompactElapsedAgo(i18n, parsed, now);
}

function describeRealtimeState(
  state: RealtimeConnectionState,
  i18n: I18nRuntime,
) {
  switch (state) {
    case "connected":
      return i18n.t("overview.realtime.connected");
    case "reconnecting":
      return i18n.t("overview.realtime.retrying");
    case "connecting":
      return i18n.t("overview.realtime.starting");
    case "closed":
      return i18n.t("overview.realtime.paused");
    default:
      return i18n.t("overview.realtime.idle");
  }
}

function describeSensorState(
  state: OverviewState,
  realtimeState: RealtimeConnectionState,
  i18n: I18nRuntime,
  now = Date.now(),
  isShowingCachedTelemetry = false,
): SensorStateCopy {
  if (!state.telemetrySnapshot) {
    return realtimeState === "connected"
      ? {
          kind: "waiting_first_sample",
          label: i18n.t("overview.realtime.waitingForFirstSample"),
        }
      : {
          kind: "waiting",
          label: i18n.t("overview.realtime.waiting"),
        };
  }

  if (isShowingCachedTelemetry) {
    const lastKnownTelemetry = formatLastSeen(state.lastTelemetryAt, i18n, now);
    const lastKnown =
      lastKnownTelemetry !== null
        ? lastKnownTelemetry
        : formatLastSeen(state.lastSuccessfulRefreshAt, i18n, now);
    return {
      kind: "cached",
      label: lastKnown
        ? `${i18n.t("overview.realtime.cached")} ${lastKnown}`
        : i18n.t("overview.realtime.cached"),
    };
  }

  if (state.telemetrySnapshot.lastReadOk === false) {
    return {
      kind: "read_failed",
      label: i18n.t("overview.realtime.readFailed"),
    };
  }

  if (!state.lastTelemetryAt) {
    return {
      kind: "sampled",
      label: i18n.t("overview.realtime.sampled"),
    };
  }

  const parsed = parseValidTimestamp(state.lastTelemetryAt);
  if (parsed === null) {
    return {
      kind: "sampled",
      label: i18n.t("overview.realtime.sampled"),
    };
  }

  const ageMs = now - parsed;
  if (ageMs > 2 * 60 * 1000) {
    return {
      kind: "stale",
      label: i18n.t("overview.realtime.stale", {
        time: formatCompactElapsedAgo(i18n, parsed, now),
      }),
    };
  }

  return {
    kind: "fresh",
    label: formatCompactElapsedAgo(i18n, parsed, now),
  };
}

function formatFirmwareLabel(state: OverviewState, i18n: I18nRuntime) {
  return (
    state.systemInfo?.firmware_version ?? i18n.t("overview.device.unknown")
  );
}

function formatWifiLabel(state: OverviewState, i18n: I18nRuntime) {
  const packet = state.systemStatus;

  if (typeof packet?.rssi === "number" && packet.isConnected) {
    return formatRssiValue(i18n, packet.rssi);
  }

  if (packet) {
    return packet.isConnected
      ? i18n.t("overview.wifi.connected")
      : i18n.t("overview.wifi.offline");
  }

  const wifiDiagnostics = state.systemStatusSnapshot?.diagnostics?.wifi;
  if (!wifiDiagnostics) {
    return i18n.t("overview.wifi.waiting");
  }

  if (typeof wifiDiagnostics.rssi === "number" && wifiDiagnostics.connected) {
    return formatRssiValue(i18n, wifiDiagnostics.rssi);
  }

  if (wifiDiagnostics.rescueApActive) {
    return i18n.t("overview.wifi.rescueAp");
  }

  if (typeof wifiDiagnostics.connected === "boolean") {
    return wifiDiagnostics.connected
      ? i18n.t("overview.wifi.connected")
      : i18n.t("overview.wifi.offline");
  }

  switch (wifiDiagnostics.state) {
    case "sta_connected":
      return i18n.t("overview.wifi.connected");
    case "sta_connecting":
    case "connecting":
      return i18n.t("overview.wifi.connecting");
    case "sta_disconnected":
    case "disconnected":
      return i18n.t("overview.wifi.offline");
    default:
      return i18n.t("overview.wifi.waiting");
  }
}

function formatRealtimeLabel(
  state: OverviewState,
  realtimeState: RealtimeConnectionState,
  i18n: I18nRuntime,
  now = Date.now(),
  isShowingCachedTelemetry = false,
) {
  const parts = [describeRealtimeState(realtimeState, i18n)];
  const sensorState = describeSensorState(
    state,
    realtimeState,
    i18n,
    now,
    isShowingCachedTelemetry,
  );

  parts.push(
    sensorState.kind === "waiting_first_sample"
      ? i18n.t("overview.realtime.waitingForData")
      : sensorState.label.toLowerCase(),
  );

  return parts.join(" • ");
}

function buildDeviceItems(
  state: OverviewState,
  session: DeviceSession | null,
  i18n: I18nRuntime,
): DetailItem[] {
  return [
    {
      id: "user",
      label: i18n.t("overview.device.user"),
      value: session?.username ?? i18n.t("overview.device.signedOut"),
    },
    {
      id: "firmware",
      label: "",
      value: formatFirmwareLabel(state, i18n),
    },
    {
      id: "uptime",
      label: i18n.t("overview.device.uptime"),
      value: state.systemInfo
        ? formatUptime(i18n, state.systemInfo.uptime)
        : i18n.t("common.waiting"),
    },
  ];
}

function buildStatusItems(
  state: OverviewState,
  realtimeState: RealtimeConnectionState,
  i18n: I18nRuntime,
  now: number,
  isShowingCachedTelemetry: boolean,
): DetailItem[] {
  return [
    {
      id: "realtime",
      label: i18n.t("overview.device.live"),
      value: formatRealtimeLabel(
        state,
        realtimeState,
        i18n,
        now,
        isShowingCachedTelemetry,
      ),
    },
    {
      id: "wifi",
      label: i18n.t("overview.device.wifi"),
      value: formatWifiLabel(state, i18n),
    },
  ];
}

export function buildDeviceSettingsViewModel(input: {
  device: DeviceRecord | null;
  session: DeviceSession | null;
  state: OverviewState;
  realtimeState: RealtimeConnectionState;
  isShowingCachedTelemetry?: boolean;
  now?: number;
  i18n?: I18nRuntime;
}): DeviceSettingsViewModel {
  if (!input.device) {
    return {
      deviceItems: [],
      statusItems: [],
    };
  }

  const now = input.now ?? Date.now();
  const i18n = input.i18n ?? getI18nRuntime();

  return {
    deviceItems: buildDeviceItems(input.state, input.session, i18n),
    statusItems: buildStatusItems(
      input.state,
      input.realtimeState,
      i18n,
      now,
      input.isShowingCachedTelemetry ?? false,
    ),
  };
}
