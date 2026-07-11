import { describe, expect, it, vi } from "vitest";

import { DeviceAlarmsApi } from "./alarms";
import { DeviceDiagnosticsApi } from "./diagnostics";
import {
  DeviceCompensationApi,
  DeviceHeartbeatApi,
  DeviceUdpApi,
} from "./integrations";
import { DeviceLogsApi } from "./logs";
import { DeviceMacrosApi } from "./macros";
import { DeviceNotificationsApi } from "./notifications";
import { DevicePowerApi } from "./power";
import type { AlarmRulesConfig, WifiSensingSettings } from "./types";
import { DeviceWifiSensingApi } from "./wifisensing";

function jsonResponse(body: unknown, init: ResponseInit = {}) {
  return new Response(JSON.stringify(body), {
    status: init.status ?? 200,
    headers: {
      "content-type": "application/json",
      ...(init.headers ?? {}),
    },
  });
}

function textResponse(body: string, init: ResponseInit = {}) {
  return new Response(body, {
    status: init.status ?? 200,
    headers: {
      "content-type": "text/plain",
      ...(init.headers ?? {}),
    },
  });
}

function byteResponse(bytes: number[], init: ResponseInit = {}) {
  return new Response(new Uint8Array(bytes), {
    status: init.status ?? 200,
    headers: init.headers,
  });
}

function createRecordingFetch(
  routes: Record<string, Response | (() => Response)>,
) {
  const calls: Array<{ url: string; init?: RequestInit }> = [];
  const fetchMock = vi.fn(
    async (input: RequestInfo | URL, init?: RequestInit) => {
      const url = String(input);
      calls.push({ url, init });
      const path = url.replace(/^https:\/\/192\.168\.0\.30/, "");
      const response = routes[path];
      if (!response) {
        return jsonResponse(
          { error: "test/not_found", message: path },
          { status: 404 },
        );
      }
      return typeof response === "function" ? response() : response;
    },
  );

  return {
    calls,
    fetch: fetchMock as unknown as typeof fetch,
  };
}

const clientOptions = (fetchImpl: typeof fetch) => ({
  baseUrl: "https://192.168.0.30",
  bearerToken: "token",
  fetch: fetchImpl,
});

describe("device SDK domain clients", () => {
  it("covers diagnostics endpoints with typed clients", async () => {
    const recording = createRecordingFetch({
      "/api/diagnostics/summary": jsonResponse({ schema: "diagnostics.v1" }),
      "/api/diagnostics/heap": jsonResponse({
        schema: "diagnostics.heap.v1",
        regions: {},
      }),
      "/api/diagnostics/tasks?details=1": jsonResponse({
        taskCount: 1,
        memory: {},
      }),
      "/api/diagnostics/mutexes": jsonResponse({
        schema: "diagnostics.mutexes.v1",
      }),
      "/api/diagnostics/endpoints": jsonResponse({
        schema: "diagnostics.endpoints.v1",
        diagnostics: [],
      }),
      "/api/diagnostics/features": jsonResponse({
        schema: "diagnostics.features.v1",
        features: [],
      }),
    });
    const api = new DeviceDiagnosticsApi(clientOptions(recording.fetch));

    await api.getSummary();
    await api.getHeap();
    await api.getTasks({ details: true });
    await api.getMutexes();
    await api.getEndpoints();
    await api.getFeatures();

    expect(recording.calls.map((call) => call.url)).toEqual([
      "https://192.168.0.30/api/diagnostics/summary",
      "https://192.168.0.30/api/diagnostics/heap",
      "https://192.168.0.30/api/diagnostics/tasks?details=1",
      "https://192.168.0.30/api/diagnostics/mutexes",
      "https://192.168.0.30/api/diagnostics/endpoints",
      "https://192.168.0.30/api/diagnostics/features",
    ]);
  });

  it("covers power, alarms, WiFi sensing and integration settings", async () => {
    const alarmConfig: AlarmRulesConfig = {
      schema_version: 1,
      rules: [
        {
          id: "csi-motion",
          enabled: true,
          name: "CSI motion",
          source: "wifi_csi_motion",
          operator: "above",
          threshold: 0.5,
          severity: "warning",
          notify_channels: ["led"],
          cooldown_seconds: 60,
          triggered: true,
          current_value: 1,
          transition_seq: 7,
          device_millis: 1234,
          boot_id: "0123456789abcdef",
        },
      ],
    };
    const wifiSensingSettings: WifiSensingSettings = {
      enabled: true,
      sample_interval_ms: 250,
      variance_threshold: 5,
      csi_alarm: {
        enabled: true,
        bands: [{ start: 10, end: 17 }],
        baseline_frames: 150,
        top_k: 8,
        enter_threshold: 6,
        clear_threshold: 3,
        hold_ms: 1200,
        clear_hold_ms: 2500,
        min_noise: 4,
        min_energy: 4,
        noisy_threshold: 80,
        auto_recalibration: true,
        sensitivity: 1,
      },
    };
    const udpSettings = {
      enabled: true,
      host: "collector.local",
      port: 9000,
      format: "json",
      interval_ms: 5000,
    };
    const heartbeatSettings = {
      interval_ms: 60000,
      slots: [
        {
          enabled: true,
          name: "health",
          url: "https://example.test",
          allow_insecure: false,
        },
      ],
    };
    const compensationSettings = {
      enabled: true,
      base_temp_offset: 1,
      reference_cpu_temp: 45,
      temp_offset_per_cpu_degree: 0.05,
      min_temp_offset: 0,
      max_temp_offset: 5,
    };
    const recording = createRecordingFetch({
      "/rest/power/status": jsonResponse({
        sleep_enabled: false,
        inactivity_timeout_ms: 0,
        grace_after_boot_ms: 0,
        wake_timer_enabled: true,
        wake_button_enabled: true,
        wake_touch_enabled: false,
        wake_interval_ms: 600000,
        timer_wake_awake_ms: 60000,
        button_wake_awake_ms: 120000,
        wake_touch_gpio: 4,
        wake_touch_threshold: 2000,
        wake_awake_window_ms: 0,
        wake_awake_eta_ms: 0,
      }),
      "/rest/power/config": jsonResponse({
        sleep_enabled: true,
        inactivity_timeout_ms: 120000,
        grace_after_boot_ms: 30000,
        wake_timer_enabled: true,
        wake_button_enabled: true,
        wake_touch_enabled: false,
        wake_interval_ms: 600000,
        timer_wake_awake_ms: 60000,
        button_wake_awake_ms: 120000,
        wake_touch_gpio: 4,
        wake_touch_threshold: 2000,
      }),
      "/api/alarms/rules?includeStatus=1": jsonResponse(alarmConfig),
      "/api/wifisensing/config": jsonResponse(wifiSensingSettings),
      "/api/wifisensing/status": jsonResponse({
        schema: "wifisensing.status.v1",
        enabled: true,
        running: true,
      }),
      "/api/wifisensing/csi/calibrate": jsonResponse({
        ok: true,
        state: "calibrating",
      }),
      "/api/heartbeat": jsonResponse(heartbeatSettings),
      "/api/heartbeat/test": jsonResponse({
        success: true,
        message: "queued",
        status: "queued",
      }),
      "/api/udp": jsonResponse(udpSettings),
      "/api/udp/test": jsonResponse({
        success: true,
        message: "sent",
        status: "sent",
      }),
      "/api/compensation": jsonResponse(compensationSettings),
    });

    await new DevicePowerApi(clientOptions(recording.fetch)).getStatus();
    await new DevicePowerApi(clientOptions(recording.fetch)).updateConfig({
      sleep_enabled: true,
    });
    const alarmRules = await new DeviceAlarmsApi(
      clientOptions(recording.fetch),
    ).getRules({
      includeStatus: true,
    });
    expect(alarmRules.rules[0]).toMatchObject({
      transition_seq: 7,
      device_millis: 1234,
      boot_id: "0123456789abcdef",
    });
    await new DeviceWifiSensingApi(
      clientOptions(recording.fetch),
    ).getSettings();
    await new DeviceWifiSensingApi(clientOptions(recording.fetch)).getStatus();
    await new DeviceWifiSensingApi(
      clientOptions(recording.fetch),
    ).calibrateCsiMotion();
    await new DeviceHeartbeatApi(clientOptions(recording.fetch)).getSettings();
    await new DeviceHeartbeatApi(clientOptions(recording.fetch)).testPing();
    await new DeviceUdpApi(clientOptions(recording.fetch)).getSettings();
    await new DeviceUdpApi(clientOptions(recording.fetch)).testSend();
    await new DeviceCompensationApi(
      clientOptions(recording.fetch),
    ).getSettings();

    expect(
      recording.calls.map(
        (call) => `${call.init?.method ?? "GET"} ${call.url}`,
      ),
    ).toEqual([
      "GET https://192.168.0.30/rest/power/status",
      "POST https://192.168.0.30/rest/power/config",
      "GET https://192.168.0.30/api/alarms/rules?includeStatus=1",
      "GET https://192.168.0.30/api/wifisensing/config",
      "GET https://192.168.0.30/api/wifisensing/status",
      "POST https://192.168.0.30/api/wifisensing/csi/calibrate",
      "GET https://192.168.0.30/api/heartbeat",
      "POST https://192.168.0.30/api/heartbeat/test",
      "GET https://192.168.0.30/api/udp",
      "POST https://192.168.0.30/api/udp/test",
      "GET https://192.168.0.30/api/compensation",
    ]);
  });

  it("parses log and macro response bodies without live device calls", async () => {
    const recording = createRecordingFetch({
      "/api/logs": jsonResponse({
        months: [{ name: "2026-06", path: "/data/2026-06", files: [] }],
      }),
      "/api/logs/download?file=%2Fdata%2F2026-06%2F2026-06-19.bin":
        byteResponse([1, 2, 3]),
      "/rest/logs/tail?lines=64&since=100": jsonResponse({
        capacity: 128,
        lines: [{ t: 101, l: "I", g: "TEST", m: "ok" }],
      }),
      "/api/macros": jsonResponse([{ name: "boot.js" }]),
      "/api/macros/content?name=boot.js": textResponse("MOVE 1 1"),
      "/api/macros/status": jsonResponse({
        current_script: "boot.js",
        status: "IDLE",
        current_line: 0,
        uptime_ms: 0,
        last_error: "",
      }),
    });
    const logs = new DeviceLogsApi(clientOptions(recording.fetch));
    const macros = new DeviceMacrosApi(clientOptions(recording.fetch));

    await expect(logs.getLogsList()).resolves.toMatchObject({
      months: [{ name: "2026-06" }],
    });
    await expect(
      logs.downloadLogBytes("/data/2026-06/2026-06-19.bin"),
    ).resolves.toHaveProperty("byteLength", 3);
    await expect(
      logs.getLogTail({ lines: 64, since: 100 }),
    ).resolves.toMatchObject({
      capacity: 128,
      lines: [{ g: "TEST", m: "ok" }],
    });
    await expect(macros.listScripts()).resolves.toEqual([{ name: "boot.js" }]);
    await expect(macros.getScriptContent("boot.js")).resolves.toBe("MOVE 1 1");
    await expect(macros.getStatus()).resolves.toMatchObject({ status: "IDLE" });
  });

  it("covers notification settings without exercising live notification sends", async () => {
    const settings = {
      telegram_enabled: false,
      webhook_enabled: true,
      bot_token: "",
      chat_id: "",
      commands_enabled: false,
      webhook_url: "https://example.test/hook",
      pushover_enabled: false,
      pushover_user: "",
      pushover_token: "",
      is_configured: true,
    };
    const recording = createRecordingFetch({
      "/api/notifications/settings": jsonResponse(settings),
    });

    await expect(
      new DeviceNotificationsApi(clientOptions(recording.fetch)).getSettings(),
    ).resolves.toEqual(settings);
    expect(recording.calls.map((call) => call.url)).toEqual([
      "https://192.168.0.30/api/notifications/settings",
    ]);
  });
});
