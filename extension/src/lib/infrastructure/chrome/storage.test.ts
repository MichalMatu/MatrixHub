import { beforeEach, describe, expect, it, vi } from "vitest";

const browserMocks = vi.hoisted(() => ({
  get: vi.fn(),
  set: vi.fn(),
}));

vi.mock("wxt/browser", () => ({
  browser: {
    storage: {
      local: {
        get: browserMocks.get,
        set: browserMocks.set,
      },
    },
  },
}));

import {
  EMPTY_EXTENSION_STATE,
  loadExtensionState,
  sanitizeExtensionState,
  saveExtensionState,
} from "./storage";
import { DEFAULT_MATRIX_SETTINGS } from "$lib/features/matrix/model";

describe("extension storage", () => {
  beforeEach(() => {
    browserMocks.get.mockReset();
    browserMocks.set.mockReset();
  });

  it("sanitizes invalid state and removes orphan sessions", () => {
    expect(
      sanitizeExtensionState({
        devices: [
          {
            id: "office",
            name: "Office",
            origin: "https://matrixhub.local",
            input: "matrixhub.local",
            createdAt: "2026-01-01T00:00:00.000Z",
          },
        ],
        selectedDeviceId: "missing",
        sessions: {
          office: {
            accessToken: "token",
            username: "admin",
            admin: true,
            signedInAt: "2026-01-01T00:00:00.000Z",
          },
          ghost: {
            accessToken: "token",
            username: "ghost",
            admin: false,
            signedInAt: "2026-01-01T00:00:00.000Z",
          },
        },
        deviceSnapshots: {
          office: {
            overviewState: {
              telemetrySnapshot: {
                co2: 501.6,
                temp: 21.4,
                humid: 48.1,
                lastReadOk: true,
              },
              systemInfo: {
                firmware_version: "2.0.0",
                firmware_name: "MatrixHub",
                core_temp: 40,
                uptime: 180,
                ignored: "skip",
              },
              systemStatusSnapshot: {
                diagnostics: {
                  wifi: {
                    healthy: true,
                    state: "sta_connected",
                    ignored: "skip",
                  },
                },
              },
              systemStatus: {
                timestamp: 1,
                lastUpdate: 2,
                wifiStatus: 3,
                rssi: -58,
                isConnected: true,
                isStaConnected: true,
                ignored: "skip",
              },
              lastSuccessfulRefreshAt: "2026-01-01T12:00:00.000Z",
              lastTelemetryAt: "2026-01-01T12:01:00.000Z",
            },
            bleStatus: {
              enabled: true,
              running: true,
              settings: {
                enabled: true,
                sensors: [
                  {
                    mac: "AA:BB:CC:DD:EE:FF",
                    alias: "Bench",
                  },
                ],
              },
              devices: [
                {
                  mac: "AA:BB:CC:DD:EE:FF",
                  temp: 21.5,
                  humid: 44.2,
                  batt: 82,
                  rssi: -61,
                  last_seen: 123,
                  ignored: "skip",
                },
              ],
            },
            shellyDevices: [
              {
                id: "relay-alpha-01",
                name: "Desk Lamp",
                isOn: true,
                isOnline: true,
                enabled: true,
                ignored: "skip",
              },
            ],
            matrixSettings: {
              brightness: 48,
              alarm_mode: 2,
              rotation: 1,
              auto_rotate: false,
              effect_enabled: false,
              effect_engine: 0,
              effect_mode: 0,
              effect_speed: 1000,
              effect_color: 65280,
              effect_color_2: 16711680,
              effect_color_3: 255,
              effect_reactivity_provider: 0,
              effect_reactivity_gain: 80,
              menu_enabled: true,
              menu_text_color: 16777215,
              menu_scroll_speed: 32,
              ignored: "skip",
            },
          },
          ghost: {
            overviewState: {
              telemetrySnapshot: {
                co2: "broken",
              },
            },
          },
        },
      }),
    ).toEqual({
      devices: [
        {
          id: "office",
          name: "Office",
          origin: "https://matrixhub.local",
          input: "matrixhub.local",
          createdAt: "2026-01-01T00:00:00.000Z",
          lastConnectedAt: undefined,
        },
      ],
      selectedDeviceId: null,
      sessions: {
        office: {
          accessToken: "token",
          username: "admin",
          admin: true,
          signedInAt: "2026-01-01T00:00:00.000Z",
        },
      },
      deviceSnapshots: {
        office: {
          overviewState: {
            telemetrySnapshot: {
              co2: 501.6,
              temp: 21.4,
              humid: 48.1,
              lastReadOk: true,
            },
            systemInfo: {
              firmware_version: "2.0.0",
              firmware_name: "MatrixHub",
              core_temp: 40,
              uptime: 180,
            },
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
              rssi: -58,
              isConnected: true,
              isStaConnected: true,
              isApMode: undefined,
              coreTemp: undefined,
            },
            lastSuccessfulRefreshAt: "2026-01-01T12:00:00.000Z",
            lastTelemetryAt: "2026-01-01T12:01:00.000Z",
          },
          bleStatus: {
            enabled: true,
            running: true,
            settings: {
              enabled: true,
              sensors: [
                {
                  mac: "aa:bb:cc:dd:ee:ff",
                  alias: "Bench",
                },
              ],
            },
            devices: [
              {
                mac: "aa:bb:cc:dd:ee:ff",
                temp: 21.5,
                humid: 44.2,
                batt: 82,
                rssi: -61,
                last_seen: 123,
              },
            ],
          },
          shellyDevices: [
            {
              id: "relay-alpha-01",
              name: "Desk Lamp",
              isOn: true,
              isOnline: true,
              enabled: true,
            },
          ],
          matrixSettings: {
            ...DEFAULT_MATRIX_SETTINGS,
            brightness: 48,
            alarm_mode: 2,
            rotation: 1,
            auto_rotate: false,
            effect_enabled: false,
            effect_engine: 0,
            effect_mode: 0,
            effect_speed: 1000,
            effect_color: 65280,
            effect_color_2: 16711680,
            effect_color_3: 255,
            effect_reactivity_provider: 0,
            effect_reactivity_gain: 80,
            menu_enabled: true,
            menu_text_color: 16777215,
            menu_scroll_speed: 32,
          },
        },
      },
      sectionVisibility: {
        matrix: true,
        bluetooth: true,
        shelly: true,
      },
    });
  });

  it("loads an empty state when storage does not contain a valid object", async () => {
    browserMocks.get.mockResolvedValue({
      "matrixhub-extension-state": "broken",
    });

    await expect(loadExtensionState()).resolves.toEqual(EMPTY_EXTENSION_STATE);
  });

  it("keeps a device snapshot when only Matrix LED settings are cached", () => {
    expect(
      sanitizeExtensionState({
        devices: [
          {
            id: "office",
            name: "Office",
            origin: "https://matrixhub.local",
            input: "matrixhub.local",
            createdAt: "2026-01-01T00:00:00.000Z",
          },
        ],
        selectedDeviceId: "office",
        sessions: {},
        deviceSnapshots: {
          office: {
            overviewState: {},
            bleStatus: null,
            shellyDevices: [],
            matrixSettings: {
              brightness: 55,
              alarm_mode: 1,
              rotation: 0,
              auto_rotate: false,
              effect_enabled: false,
              effect_engine: 0,
              effect_mode: 0,
              effect_speed: 1000,
              effect_color: 65280,
              effect_color_2: 16711680,
              effect_color_3: 255,
              effect_reactivity_provider: 0,
              effect_reactivity_gain: 80,
              menu_enabled: true,
              menu_text_color: 16777215,
              menu_scroll_speed: 30,
            },
          },
        },
      }).deviceSnapshots.office,
    ).toEqual({
      overviewState: {
        telemetrySnapshot: null,
        systemInfo: null,
        systemStatusSnapshot: null,
        systemStatus: null,
        lastSuccessfulRefreshAt: null,
        lastTelemetryAt: null,
      },
      bleStatus: null,
      shellyDevices: [],
      matrixSettings: {
        ...DEFAULT_MATRIX_SETTINGS,
        brightness: 55,
        alarm_mode: 1,
        rotation: 0,
        auto_rotate: false,
        effect_enabled: false,
        effect_engine: 0,
        effect_mode: 0,
        effect_speed: 1000,
        effect_color: 65280,
        effect_color_2: 16711680,
        effect_color_3: 255,
        effect_reactivity_provider: 0,
        effect_reactivity_gain: 80,
        menu_enabled: true,
        menu_text_color: 16777215,
        menu_scroll_speed: 30,
      },
    });
  });

  it("sanitizes remembered section visibility with safe defaults", () => {
    expect(
      sanitizeExtensionState({
        devices: [],
        selectedDeviceId: null,
        sessions: {},
        deviceSnapshots: {},
        sectionVisibility: {
          matrix: false,
          bluetooth: "broken",
        },
      }).sectionVisibility,
    ).toEqual({
      matrix: false,
      bluetooth: true,
      shelly: true,
    });
  });

  it("persists the current state under the stable storage key", async () => {
    const state = {
      devices: [],
      selectedDeviceId: null,
      sessions: {},
      deviceSnapshots: {},
      sectionVisibility: {
        matrix: false,
        bluetooth: true,
        shelly: false,
      },
    };

    await saveExtensionState(state);

    expect(browserMocks.set).toHaveBeenCalledWith({
      "matrixhub-extension-state": state,
    });
  });
});
