import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import {
  buildDeviceSettingsViewModel,
  buildOverviewCardViewModel,
} from "./viewModels";
import type { OverviewState } from "../state/overviewState";

function createState(overrides: Partial<OverviewState> = {}): OverviewState {
  return {
    telemetrySnapshot: null,
    systemInfo: null,
    systemStatusSnapshot: null,
    systemStatus: null,
    lastSuccessfulRefreshAt: null,
    lastTelemetryAt: null,
    ...overrides,
  };
}

describe("overview view models", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date("2026-01-01T12:00:00.000Z"));
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("prefers live telemetry and includes quality details", () => {
    const viewModel = buildOverviewCardViewModel({
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
        },
        lastTelemetryAt: "2026-01-01T11:59:30.000Z",
        lastSuccessfulRefreshAt: "2026-01-01T11:58:00.000Z",
      }),
    });

    expect(viewModel.details).toEqual([
      { id: "co2", label: "CO2", value: "612 ppm" },
      { id: "temperature", label: "Temp", value: "23.4 C" },
      { id: "humidity", label: "Humidity", value: "56.1 %" },
    ]);
  });

  it("stays empty until live telemetry is available", () => {
    const viewModel = buildOverviewCardViewModel({
      state: createState({
        systemInfo: {
          firmware_version: "2.0.0",
          core_temp: 41.26,
          uptime: 7322,
        } as OverviewState["systemInfo"],
        lastSuccessfulRefreshAt: "2026-01-01T11:57:00.000Z",
      }),
    });

    expect(viewModel.details).toEqual([]);
    expect(viewModel.metrics).toEqual([]);
  });

  it("builds normalized sparkline metrics for the device card", () => {
    const viewModel = buildOverviewCardViewModel({
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
          history: {
            timestamps: [100, 200, 300, 400],
            co2: [600, null, 610, 612],
            temp: [22.4, 22.9, 23.2, 23.45],
            humid: [55.0, 56.0, null, 56.11],
          },
        },
      }),
    });

    expect(viewModel.metrics).toEqual([
      {
        id: "co2",
        label: "CO2",
        value: 612.2,
        history: [600, 610, 612],
        timestamps: [100, 300, 400],
        chartColor: "#22c55e",
      },
      {
        id: "temperature",
        label: "Temp",
        value: 23.45,
        history: [22.4, 22.9, 23.2, 23.45],
        timestamps: [100, 200, 300, 400],
        chartColor: "#ef4444",
      },
      {
        id: "humidity",
        label: "Humidity",
        value: 56.11,
        history: [55.0, 56.0, 56.11],
        timestamps: [100, 200, 400],
        chartColor: "#3b82f6",
        domainMin: 0,
        domainMax: 100,
      },
    ]);
  });

  it("builds compact device and status sections for the selected device", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };
    const session: DeviceSession = {
      accessToken: "token",
      username: "admin",
      admin: true,
      signedInAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session,
      state: createState({
        systemInfo: {
          firmware_name: "MatrixHub",
          firmware_version: "2.0.0",
          core_temp: 40,
          uptime: 180,
        } as OverviewState["systemInfo"],
        systemStatusSnapshot: {
          diagnostics: {
            wifi: {
              healthy: true,
              state: "sta_connected",
            },
          },
        },
        systemStatus: {
          timestamp: 0,
          lastUpdate: 0,
          wifiStatus: 0,
          rssi: -58,
          isConnected: true,
          isStaConnected: true,
          isApMode: false,
          coreTemp: 40,
        },
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
        },
        lastTelemetryAt: "2026-01-01T11:59:00.000Z",
        lastSuccessfulRefreshAt: "2026-01-01T11:58:00.000Z",
      }),
      realtimeState: "connected",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.deviceItems).toEqual([
      { id: "user", label: "User", value: "admin" },
      { id: "firmware", label: "", value: "2.0.0" },
      { id: "uptime", label: "Uptime", value: "3m" },
    ]);
    expect(viewModel.statusItems).toEqual([
      {
        id: "realtime",
        label: "Live",
        value: "Connected • 1m ago",
      },
      {
        id: "wifi",
        label: "Wi-Fi",
        value: "-58 dBm",
      },
    ]);
  });

  it("keeps sub-minute freshness in the compact live status", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
        },
        lastTelemetryAt: "2026-01-01T11:59:45.000Z",
      }),
      realtimeState: "connected",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "realtime",
      label: "Live",
      value: "Connected • <1m ago",
    });
  });

  it("ignores legacy epoch timestamps when building compact live status", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
        },
        lastTelemetryAt: "1970-01-01T00:20:00.000Z",
      }),
      realtimeState: "connected",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "realtime",
      label: "Live",
      value: "Connected • sampled",
    });
  });

  it("falls back to Wi-Fi diagnostics from the system snapshot", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        systemStatusSnapshot: {
          diagnostics: {
            wifi: {
              connected: true,
              rssi: -61,
              state: "sta_connected",
            },
          },
        },
      }),
      realtimeState: "connecting",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "wifi",
      label: "Wi-Fi",
      value: "-61 dBm",
    });
  });

  it("marks cached telemetry as last known while live refresh is pending", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
        },
        lastTelemetryAt: "2026-01-01T11:59:00.000Z",
      }),
      realtimeState: "connecting",
      isShowingCachedTelemetry: true,
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "realtime",
      label: "Live",
      value: "Starting • cached 1m ago",
    });
  });

  it("shows waiting for data after the socket connects before the first sample arrives", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState(),
      realtimeState: "connected",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "realtime",
      label: "Live",
      value: "Connected • waiting for data",
    });
  });

  it("marks failed telemetry reads in the compact live status", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: false,
        },
      }),
      realtimeState: "connected",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "realtime",
      label: "Live",
      value: "Connected • read failed",
    });
  });

  it("marks telemetry as stale when the latest sample is too old", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        telemetrySnapshot: {
          co2: 612.2,
          temp: 23.45,
          humid: 56.11,
          lastReadOk: true,
        },
        lastTelemetryAt: "2026-01-01T11:56:00.000Z",
      }),
      realtimeState: "connected",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "realtime",
      label: "Live",
      value: "Connected • stale 4m ago",
    });
  });

  it("reports rescue access point status from diagnostics", () => {
    const device: DeviceRecord = {
      id: "office",
      name: "Office",
      origin: "https://matrixhub.local",
      input: "matrixhub.local",
      createdAt: "2026-01-01T00:00:00.000Z",
    };

    const viewModel = buildDeviceSettingsViewModel({
      device,
      session: null,
      state: createState({
        systemStatusSnapshot: {
          diagnostics: {
            wifi: {
              rescueApActive: true,
            },
          },
        },
      }),
      realtimeState: "idle",
      now: Date.parse("2026-01-01T12:00:00.000Z"),
    });

    expect(viewModel.statusItems).toContainEqual({
      id: "wifi",
      label: "Wi-Fi",
      value: "Rescue AP",
    });
  });
});
