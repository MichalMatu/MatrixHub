import { beforeEach, describe, expect, it, vi } from "vitest";
import type { MatrixSettings, SystemInformation } from "@matrixhub/device-sdk";
import {
  completeSignInFlow,
  restoreSelectedSessionFlow,
} from "$lib/features/auth/actions/sessionFlows";
import type { DeviceOverviewSocketHandlers } from "$lib/features/realtime/socket/deviceOverviewSocket";
import type { ExtensionState } from "$lib/infrastructure/chrome/storage";
import {
  buildRemoveDeviceContext,
  clearSelectedSessionContext,
} from "$lib/features/devices/actions/deviceSelection";
import { markDeviceConnected } from "$lib/domain/device/rules";
import { DEFAULT_MATRIX_SETTINGS } from "$lib/features/matrix/model";
import { createSidepanelController } from "./sidepanelController";
import type { SidepanelControllerTransition } from "./sidepanelControllerStore";
import { createInitialSidepanelState } from "../state/sidepanelState";

const officeDevice = {
  id: "office",
  name: "Office",
  origin: "https://office.local",
  input: "office.local",
  createdAt: "2026-01-01T00:00:00.000Z",
};

const labDevice = {
  id: "lab",
  name: "Lab",
  origin: "https://lab.local",
  input: "lab.local",
  createdAt: "2026-01-01T00:00:00.000Z",
};

const officeSession = {
  accessToken: "token",
  username: "admin",
  admin: true,
  signedInAt: "2026-01-01T00:00:00.000Z",
};

const officeShellyDevices = [
  {
    id: "relay-alpha-01",
    name: "Desk Lamp",
    isOn: false,
    isOnline: true,
    enabled: true,
  },
];

const labShellyDevices = [
  {
    id: "relay-beta-02",
    name: "Pump",
    isOn: true,
    isOnline: true,
    enabled: true,
  },
];

const officeCachedSnapshot = {
  overviewState: {
    telemetrySnapshot: {
      co2: 590,
      temp: 23.1,
      humid: 51.2,
      lastReadOk: true,
    },
    systemInfo: {
      firmware_version: "2.0.0",
      firmware_name: "MatrixHub",
      uptime: 120,
      core_temp: 38,
    } as SystemInformation,
    systemStatusSnapshot: {
      diagnostics: {
        wifi: {
          healthy: true,
          state: "sta_connected",
        },
      },
    },
    systemStatus: {
      timestamp: 1,
      lastUpdate: 2,
      wifiStatus: 3,
      rssi: -56,
      isConnected: true,
      isStaConnected: true,
    },
    lastSuccessfulRefreshAt: "2026-01-01T11:58:00.000Z",
    lastTelemetryAt: "2026-01-01T11:59:00.000Z",
  },
  bleStatus: {
    enabled: true,
    running: true,
    settings: {
      enabled: true,
      sensors: [{ mac: "aa:bb:cc:dd:ee:ff", alias: "Bench" }],
    },
    devices: [
      {
        mac: "aa:bb:cc:dd:ee:ff",
        temp: 21.7,
        humid: 44.5,
        batt: 79,
        rssi: -60,
        last_seen: 123,
      },
    ],
  },
  shellyDevices: officeShellyDevices,
  matrixSettings: {
    ...DEFAULT_MATRIX_SETTINGS,
    brightness: 36,
    alarm_mode: 1,
    rotation: 0,
    auto_rotate: false,
    effect_enabled: false,
    effect_engine: 0,
    effect_mode: 0,
    effect_speed: 1000,
    effect_color: 0x00ff00,
    effect_color_2: 0xff0000,
    effect_color_3: 0x0000ff,
    effect_reactivity_provider: 0,
    effect_reactivity_gain: 80,
    menu_enabled: true,
    menu_text_color: 0xffffff,
    menu_scroll_speed: 28,
  },
};

const labCachedSnapshot = {
  overviewState: {
    telemetrySnapshot: {
      co2: 610,
      temp: 24.2,
      humid: 49.1,
      lastReadOk: true,
    },
    systemInfo: {
      firmware_version: "2.0.0",
      uptime: 240,
      core_temp: 39,
    } as SystemInformation,
    systemStatusSnapshot: null,
    systemStatus: null,
    lastSuccessfulRefreshAt: "2026-01-01T11:57:00.000Z",
    lastTelemetryAt: "2026-01-01T11:58:00.000Z",
  },
  bleStatus: null,
  shellyDevices: labShellyDevices,
  matrixSettings: {
    ...DEFAULT_MATRIX_SETTINGS,
    brightness: 64,
    alarm_mode: 0,
    rotation: 1,
    auto_rotate: false,
    effect_enabled: false,
    effect_engine: 0,
    effect_mode: 0,
    effect_speed: 1000,
    effect_color: 0x0000ff,
    effect_color_2: 0x00ff00,
    effect_color_3: 0xff0000,
    effect_reactivity_provider: 0,
    effect_reactivity_gain: 80,
    menu_enabled: false,
    menu_text_color: 0xffeeaa,
    menu_scroll_speed: 44,
  },
};

const shellyDevices = officeShellyDevices;

const matrixSettings: MatrixSettings = {
  ...DEFAULT_MATRIX_SETTINGS,
  brightness: 48,
  alarm_mode: 2,
  rotation: 0,
  auto_rotate: false,
  effect_enabled: false,
  effect_engine: 0,
  effect_mode: 0,
  effect_speed: 1000,
  effect_color: 0x00ff00,
  effect_color_2: 0xff0000,
  effect_color_3: 0x0000ff,
  effect_reactivity_provider: 0,
  effect_reactivity_gain: 80,
  menu_enabled: true,
  menu_text_color: 0xffffff,
  menu_scroll_speed: 32,
};

function createDeferred<T>() {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((nextResolve) => {
    resolve = nextResolve;
  });

  return {
    promise,
    resolve,
  };
}

function createHarness() {
  let state = createInitialSidepanelState();
  let socketHandlers: DeviceOverviewSocketHandlers | null = null;
  const transitions: SidepanelControllerTransition[] = [];
  const socket = {
    connect: vi.fn(),
    disconnect: vi.fn(),
  };

  const deps = {
    afterSessionApplied: vi.fn(async () => undefined),
    loadExtensionState: vi.fn<() => Promise<ExtensionState>>(async () => ({
      devices: [],
      selectedDeviceId: null,
      sessions: {},
      deviceSnapshots: {},
      sectionVisibility: {
        matrix: true,
        bluetooth: true,
        shelly: true,
      },
    })),
    saveExtensionState: vi.fn(async () => undefined),
    syncWsAccessTokenCookie: vi.fn(async () => undefined),
    clearWsAccessTokenCookie: vi.fn(async () => undefined),
    saveDeviceDraft: vi.fn(),
    signInSelectedDevice: vi.fn(),
    restoreSelectedSessionFlow: vi.fn(restoreSelectedSessionFlow),
    completeSignInFlow: vi.fn(completeSignInFlow),
    createSystemApiForSelection: vi.fn((context) => ({ ...context })),
    createBleApiForSelection: vi.fn(() => ({
      getStatus: vi.fn(async () => ({
        enabled: true,
        running: true,
        scanner_active: false,
        settings: {
          enabled: true,
          sensors: [],
        },
        devices: [],
      })),
      saveSettings: vi.fn(
        async (settings: {
          enabled?: boolean;
          sensors?: Array<{ mac: string; alias: string }>;
        }) => ({
          enabled: settings.enabled ?? true,
          sensors: settings.sensors ?? [],
        }),
      ),
      startScan: vi.fn(async () => undefined),
      stopScan: vi.fn(async () => undefined),
    })),
    createShellyApiForSelection: vi.fn(() => ({
      getDevices: vi.fn(async () => shellyDevices),
      setRelayState: vi.fn(async () => undefined),
    })),
    createMatrixApiForSelection: vi.fn(() => ({
      getSettings: vi.fn(async () => matrixSettings),
      updateSettings: vi.fn(async (settings: Partial<MatrixSettings>) => ({
        ...(state.matrixSettings ?? matrixSettings),
        ...settings,
      })),
    })),
    fetchOverviewSnapshot: vi.fn(async () => ({
      connectedAt: "2026-01-01T12:00:00.000Z",
      systemInfo: {
        firmware_version: "2.0.0",
        core_temp: 40,
        uptime: 180,
      } as SystemInformation,
    })),
    requestWifiRecoveryMessage: vi.fn(async () => "Reconnect requested."),
    buildRemoveDeviceContext,
    clearSelectedSessionContext,
    markDeviceConnected,
    createOverviewSocket: vi.fn((handlers: DeviceOverviewSocketHandlers) => {
      socketHandlers = handlers;
      return socket;
    }),
  };

  const onInfo = vi.fn();
  const onError = vi.fn();
  const clearMessages = vi.fn();

  const controller = createSidepanelController({
    state,
    onStateChange: (nextState) => {
      state = nextState;
    },
    onInfo,
    onError,
    clearMessages,
    onTransition: (transition) => {
      transitions.push(transition);
    },
    deps,
  });

  return {
    controller,
    deps,
    socket,
    getState: () => state,
    getSocketHandlers: () => socketHandlers,
    onInfo,
    onError,
    clearMessages,
    getTransitions: () => transitions,
  };
}

describe("sidepanel controller", () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it("bootstraps saved state and opens the add sheet when there are no devices", async () => {
    const harness = createHarness();

    await harness.controller.bootstrap();

    expect(harness.getState().isBootstrapping).toBe(false);
    expect(harness.getState().isAddSheetOpen).toBe(true);
  });

  it("hydrates the selected device from cache before live refresh finishes", async () => {
    const harness = createHarness();
    const refreshDeferred = createDeferred<{
      connectedAt: string;
      systemInfo: SystemInformation;
    }>();

    harness.deps.loadExtensionState.mockResolvedValue({
      devices: [officeDevice],
      selectedDeviceId: officeDevice.id,
      sessions: {
        [officeDevice.id]: officeSession,
      },
      deviceSnapshots: {
        [officeDevice.id]: officeCachedSnapshot,
      },
      sectionVisibility: {
        matrix: false,
        bluetooth: true,
        shelly: false,
      },
    });
    harness.deps.restoreSelectedSessionFlow.mockImplementation(
      async (input: Parameters<typeof restoreSelectedSessionFlow>[0]) => {
        input.resetOverviewState();
        await input.hydrateRealtimeAuth();
        input.connectRealtime();
        await input.refreshOverview();
        return true;
      },
    );
    harness.deps.fetchOverviewSnapshot.mockImplementation(
      async () => refreshDeferred.promise,
    );

    await harness.controller.bootstrap();

    expect(harness.getState().overviewState.telemetrySnapshot).toMatchObject({
      co2: 590,
    });
    expect(harness.getState().bleStatus?.devices).toHaveLength(1);
    expect(harness.getState().selectedDeviceDataOrigins).toEqual({
      overview: "cache",
      telemetry: "cache",
      ble: "cache",
      shelly: "cache",
      matrix: "cache",
    });
    expect(harness.getState().sectionVisibility).toEqual({
      matrix: false,
      bluetooth: true,
      shelly: false,
    });

    refreshDeferred.resolve({
      connectedAt: "2026-01-01T12:00:00.000Z",
      systemInfo: {
        firmware_version: "2.0.0",
        core_temp: 40,
        uptime: 180,
      } as SystemInformation,
    });

    await vi.waitFor(() => {
      expect(harness.getState().selectedDeviceDataOrigins).toEqual({
        overview: "live",
        telemetry: "cache",
        ble: "live",
        shelly: "live",
        matrix: "live",
      });
    });
    expect(harness.getState().shellyDevices).toEqual(shellyDevices);
    expect(harness.getState().matrixSettings).toEqual(matrixSettings);
    expect(harness.socket.connect).toHaveBeenCalledWith(officeDevice.origin);
  });

  it("adds a device and clears the draft after a successful save", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.deviceDraft.name = "Office";
    state.deviceDraft.address = "office.local";
    harness.deps.saveDeviceDraft.mockResolvedValue({
      kind: "success",
      devices: [officeDevice],
      selectedDeviceId: officeDevice.id,
    });

    await harness.controller.addDevice();

    expect(harness.getState().devices).toEqual([officeDevice]);
    expect(harness.getState().selectedDeviceId).toBe(officeDevice.id);
    expect(harness.getState().deviceDraft).toEqual({
      name: "",
      address: "",
    });
    expect(harness.onInfo).toHaveBeenCalledWith(
      "Device saved. Sign in to load the live overview.",
    );
  });

  it("signs in, refreshes overview data and connects realtime updates", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.credentials.username = "admin";
    state.credentials.password = "secret";
    harness.deps.signInSelectedDevice.mockResolvedValue({
      kind: "success",
      session: officeSession,
    });

    await harness.controller.signIn();

    expect(harness.getState().sessions[officeDevice.id]).toEqual(officeSession);
    expect(harness.getState().credentials.password).toBe("");
    expect(harness.socket.connect).toHaveBeenCalledWith(officeDevice.origin);
    expect(harness.getState().overviewState.systemInfo).toMatchObject({
      firmware_version: "2.0.0",
    });
    expect(harness.getState().shellyDevices).toEqual(shellyDevices);
    expect(harness.getState().matrixSettings).toEqual(matrixSettings);
    expect(harness.getState().deviceSnapshots[officeDevice.id]).toBeDefined();
    expect(harness.onInfo).toHaveBeenCalledWith(
      "Signed in. Restoring overview and live telemetry.",
    );
  });

  it("keeps restoring signed-in data when overview refresh fails", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.credentials.username = "admin";
    state.credentials.password = "secret";
    harness.deps.signInSelectedDevice.mockResolvedValue({
      kind: "success",
      session: officeSession,
    });
    harness.deps.fetchOverviewSnapshot.mockRejectedValue(
      new Error("overview failed"),
    );

    await harness.controller.signIn();

    expect(harness.socket.connect).toHaveBeenCalledWith(officeDevice.origin);
    expect(harness.getState().sessions[officeDevice.id]).toEqual(officeSession);
    expect(harness.getState().bleStatus).not.toBeNull();
    expect(harness.getState().shellyDevices).toEqual(shellyDevices);
    expect(harness.getState().matrixSettings).toEqual(matrixSettings);
    expect(harness.onError).toHaveBeenCalledWith(expect.any(Error));
    expect(harness.onInfo).toHaveBeenCalledWith(
      "Signed in. Restoring overview and live telemetry.",
    );
  });

  it("saves minimal Matrix LED settings through the selected device session", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.matrixSettings = matrixSettings;

    await harness.controller.saveMatrixSettings({
      brightness: 77,
      alarm_mode: 1,
      menu_enabled: false,
      menu_scroll_speed: 24,
    });

    const matrixApi =
      harness.deps.createMatrixApiForSelection.mock.results[0]?.value;
    expect(matrixApi?.updateSettings).toHaveBeenCalledWith({
      brightness: 77,
      alarm_mode: 1,
      menu_enabled: false,
      menu_scroll_speed: 24,
    });
    expect(harness.getState().matrixSettings).toMatchObject({
      brightness: 77,
      alarm_mode: 1,
      menu_enabled: false,
      menu_scroll_speed: 24,
    });
    expect(harness.getState().activity.isSavingMatrixSettings).toBe(false);
    expect(harness.onInfo).toHaveBeenCalledWith("Matrix LED settings saved.");
  });

  it("preserves Matrix LED effect state when brightness is saved as zero", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.matrixSettings = {
      ...matrixSettings,
      effect_enabled: true,
    };

    await harness.controller.saveMatrixSettings({
      brightness: 0,
    });

    const matrixApi =
      harness.deps.createMatrixApiForSelection.mock.results[0]?.value;
    expect(matrixApi?.updateSettings).toHaveBeenCalledWith({
      brightness: 0,
    });
    expect(harness.getState().matrixSettings).toMatchObject({
      brightness: 0,
      effect_enabled: true,
    });
  });

  it("keeps Matrix LED effect state active after restoring brightness above zero", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.matrixSettings = {
      ...matrixSettings,
      brightness: 64,
      effect_enabled: true,
    };

    await harness.controller.saveMatrixSettings({
      brightness: 0,
    });

    await harness.controller.saveMatrixSettings({
      brightness: 64,
    });

    const firstMatrixApi =
      harness.deps.createMatrixApiForSelection.mock.results[0]?.value;
    const secondMatrixApi =
      harness.deps.createMatrixApiForSelection.mock.results[1]?.value;
    expect(firstMatrixApi?.updateSettings).toHaveBeenCalledWith({
      brightness: 0,
    });
    expect(secondMatrixApi?.updateSettings).toHaveBeenCalledWith({
      brightness: 64,
    });
    expect(harness.getState().matrixSettings).toMatchObject({
      brightness: 64,
      effect_enabled: true,
    });
  });

  it("keeps existing toasts during quiet Matrix LED quick-save", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.matrixSettings = matrixSettings;

    await harness.controller.saveMatrixSettings(
      {
        brightness: 42,
      },
      { notify: false },
    );

    expect(harness.clearMessages).not.toHaveBeenCalled();
    expect(harness.onInfo).not.toHaveBeenCalledWith(
      "Matrix LED settings saved.",
    );
  });

  it("emits a named transition trace for Matrix LED saves", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.matrixSettings = matrixSettings;

    await harness.controller.saveMatrixSettings({
      brightness: 73,
    });

    expect(
      harness.getTransitions().map((transition) => ({
        label: transition.label,
        persist: transition.persist,
      })),
    ).toEqual([
      {
        label: "matrix.save.started",
        persist: "none",
      },
      {
        label: "matrix.save.applied",
        persist: "await",
      },
      {
        label: "matrix.save.finished",
        persist: "none",
      },
    ]);
  });

  it("logs out the active session when an API context becomes unauthorized", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };

    await harness.controller.refreshOverview();
    const apiContext =
      harness.deps.createSystemApiForSelection.mock.calls[0]?.[0];
    apiContext.onUnauthorized();

    await vi.waitFor(() => {
      expect(harness.getState().sessions[officeDevice.id]).toBeUndefined();
      expect(harness.deps.clearWsAccessTokenCookie).toHaveBeenCalledWith(
        officeDevice.origin,
      );
      expect(harness.onInfo).toHaveBeenCalledWith(
        "Session expired. Sign in again.",
      );
    });
  });

  it("persists remembered section visibility changes", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;

    await harness.controller.setSectionOpen("bluetooth", false);

    expect(harness.getState().sectionVisibility).toEqual({
      matrix: true,
      bluetooth: false,
      shelly: true,
    });
    await vi.waitFor(() => {
      expect(harness.deps.saveExtensionState).toHaveBeenLastCalledWith({
        devices: [officeDevice],
        selectedDeviceId: officeDevice.id,
        sessions: {},
        deviceSnapshots: {},
        sectionVisibility: {
          matrix: true,
          bluetooth: false,
          shelly: true,
        },
      });
    });
  });

  it("renames a device title and persists the updated list", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;

    const renamed = await harness.controller.renameDevice(
      officeDevice.id,
      "Pokoj dzienny",
    );

    expect(renamed).toBe(true);
    expect(harness.getState().devices).toEqual([
      {
        ...officeDevice,
        name: "Pokoj dzienny",
      },
    ]);
    await vi.waitFor(() => {
      expect(harness.deps.saveExtensionState).toHaveBeenLastCalledWith({
        devices: [
          {
            ...officeDevice,
            name: "Pokoj dzienny",
          },
        ],
        selectedDeviceId: officeDevice.id,
        sessions: {},
        deviceSnapshots: {},
        sectionVisibility: {
          matrix: true,
          bluetooth: true,
          shelly: true,
        },
      });
    });
    expect(harness.onInfo).toHaveBeenCalledWith("Device title saved.");
  });

  it("rejects an empty device title during rename", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;

    const renamed = await harness.controller.renameDevice(
      officeDevice.id,
      "   ",
    );

    expect(renamed).toBe(false);
    expect(harness.getState().devices).toEqual([officeDevice]);
    expect(harness.deps.saveExtensionState).not.toHaveBeenCalled();
    expect(harness.onError).toHaveBeenCalledWith(
      "Device title cannot be empty.",
    );
  });

  it("removes the selected device and focuses the next available record", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice, labDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.deviceSnapshots = {
      [officeDevice.id]: officeCachedSnapshot,
      [labDevice.id]: labCachedSnapshot,
    };

    await harness.controller.removeDevice(officeDevice.id);

    expect(harness.getState().devices).toEqual([labDevice]);
    expect(harness.getState().selectedDeviceId).toBe(labDevice.id);
    expect(harness.getState().deviceSnapshots).toEqual({
      [labDevice.id]: labCachedSnapshot,
    });
    expect(harness.deps.clearWsAccessTokenCookie).toHaveBeenCalledWith(
      officeDevice.origin,
    );
  });

  it("restores cached Matrix LED settings when switching devices", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice, labDevice];
    state.selectedDeviceId = officeDevice.id;
    state.deviceSnapshots = {
      [officeDevice.id]: officeCachedSnapshot,
      [labDevice.id]: labCachedSnapshot,
    };

    await harness.controller.selectDevice(labDevice.id);

    expect(harness.getState().matrixSettings).toEqual(
      labCachedSnapshot.matrixSettings,
    );
    expect(harness.getState().shellyDevices).toEqual(labShellyDevices);
    expect(harness.getState().selectedDeviceDataOrigins).toEqual({
      overview: "cache",
      telemetry: "cache",
      ble: "empty",
      shelly: "cache",
      matrix: "cache",
    });
  });

  it("applies socket updates to the current overview state and updates cache write-through", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    const handlers = harness.getSocketHandlers();
    if (!handlers) {
      throw new Error("Missing socket handlers");
    }

    handlers.onTelemetrySnapshot?.({
      co2: 500,
      temp: 22,
      humid: 50,
      lastReadOk: true,
    });

    expect(harness.getState().overviewState.telemetrySnapshot).toMatchObject({
      co2: 500,
    });
    await Promise.resolve();
    expect(
      harness.getState().deviceSnapshots[officeDevice.id]?.overviewState
        .telemetrySnapshot,
    ).toMatchObject({
      co2: 500,
    });
  });

  it("queues Shelly relay control from the selected device card", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.shellyDevices = shellyDevices;

    await harness.controller.toggleShelly("relay-alpha-01", true);

    const shellyApi =
      harness.deps.createShellyApiForSelection.mock.results[0]?.value;
    expect(shellyApi?.setRelayState).toHaveBeenCalledWith(
      "relay-alpha-01",
      true,
    );
    expect(harness.getState().shellyDevices).toEqual([
      {
        ...shellyDevices[0],
        isOn: true,
      },
    ]);
    expect(harness.getState().activity.pendingShellyDeviceId).toBeNull();
    expect(harness.onInfo).toHaveBeenCalledWith("Shelly command queued.");
  });

  it("writes Shelly socket updates through to the device cache", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    const handlers = harness.getSocketHandlers();
    if (!handlers) {
      throw new Error("Missing socket handlers");
    }

    handlers.onShellySnapshot?.(shellyDevices);

    expect(harness.getState().shellyDevices).toEqual(shellyDevices);
    await Promise.resolve();
    expect(
      harness.getState().deviceSnapshots[officeDevice.id]?.shellyDevices,
    ).toEqual(shellyDevices);
    expect(harness.getState().selectedDeviceDataOrigins).toEqual({
      overview: "empty",
      telemetry: "empty",
      ble: "empty",
      shelly: "live",
      matrix: "empty",
    });
  });

  it("ignores a stale Matrix LED refresh that resolves after a newer save", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.matrixSettings = officeCachedSnapshot.matrixSettings;

    const refreshDeferred = createDeferred<MatrixSettings>();
    const matrixApi = {
      getSettings: vi.fn(async () => refreshDeferred.promise),
      updateSettings: vi.fn(
        async (settings: Partial<MatrixSettings>) =>
          ({
            ...matrixSettings,
            ...settings,
          }) satisfies MatrixSettings,
      ),
    };
    harness.deps.createMatrixApiForSelection.mockImplementation(
      () => matrixApi,
    );

    const refreshPromise = harness.controller.refreshMatrixSettings();
    await Promise.resolve();

    await harness.controller.saveMatrixSettings({
      brightness: 77,
      alarm_mode: 1,
      menu_enabled: false,
      menu_scroll_speed: 24,
    });

    refreshDeferred.resolve({
      ...matrixSettings,
      brightness: 12,
      alarm_mode: 2,
      menu_enabled: true,
      menu_scroll_speed: 99,
    });
    await refreshPromise;

    expect(harness.getState().matrixSettings).toMatchObject({
      brightness: 77,
      alarm_mode: 1,
      menu_enabled: false,
      menu_scroll_speed: 24,
    });
  });

  it("keeps device cache across logout", async () => {
    const harness = createHarness();
    const state = harness.getState();
    state.devices = [officeDevice];
    state.selectedDeviceId = officeDevice.id;
    state.sessions = {
      [officeDevice.id]: officeSession,
    };
    state.deviceSnapshots = {
      [officeDevice.id]: officeCachedSnapshot,
    };

    await harness.controller.logout();

    expect(harness.getState().deviceSnapshots).toEqual({
      [officeDevice.id]: officeCachedSnapshot,
    });
    expect(harness.deps.saveExtensionState).toHaveBeenLastCalledWith({
      devices: [officeDevice],
      selectedDeviceId: officeDevice.id,
      sessions: {},
      deviceSnapshots: {
        [officeDevice.id]: officeCachedSnapshot,
      },
      sectionVisibility: {
        matrix: true,
        bluetooth: true,
        shelly: true,
      },
    });
  });
});
