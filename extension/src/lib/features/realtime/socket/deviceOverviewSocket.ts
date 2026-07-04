import {
  parseBleEventPacket,
  parseBleStatusSnapshot,
  buildDeviceWebSocketUrl,
  parseShellyEventPacket,
  parseSensorTelemetryPacket,
  parseShellySnapshot,
  parseRealtimeEnvelopeFrame,
  parseSensorTelemetryEvent,
  parseSystemStatusPacket,
  parseSystemStatusSnapshot,
  parseTelemetrySnapshot,
  type BleDeviceEvent,
  type BleStatus,
  type ShellyDevice,
  type ShellyDeviceEvent,
  type SensorTelemetryEvent,
  type SystemStatus,
  type SystemStatusSnapshot,
  type TelemetrySnapshot,
} from "@matrixhub/device-sdk";

export type RealtimeConnectionState =
  | "idle"
  | "connecting"
  | "connected"
  | "reconnecting"
  | "closed";

export interface DeviceOverviewSocketHandlers {
  onConnectionStateChange?: (state: RealtimeConnectionState) => void;
  onTelemetrySnapshot?: (snapshot: TelemetrySnapshot) => void;
  onTelemetryEvent?: (event: SensorTelemetryEvent) => void;
  onSystemSnapshot?: (snapshot: SystemStatusSnapshot) => void;
  onSystemPacket?: (packet: SystemStatus) => void;
  onBleSnapshot?: (snapshot: BleStatus) => void;
  onBleEvent?: (event: BleDeviceEvent) => void;
  onShellySnapshot?: (snapshot: ShellyDevice[]) => void;
  onShellyEvent?: (event: ShellyDeviceEvent) => void;
  onError?: (error: unknown) => void;
}

export class DeviceOverviewSocket {
  private ws: WebSocket | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private manualClose = false;
  private reconnectAttempt = 0;
  private origin: string | null = null;
  private connectionId = 0;

  constructor(private readonly handlers: DeviceOverviewSocketHandlers) {}

  connect(origin: string) {
    // connect() is intentionally idempotent from the caller's perspective:
    // selectDevice/signIn/restoreSession can call it after resetting state and
    // the socket manager will own the reconnect lifecycle from there.
    const alreadyActive =
      this.origin === origin &&
      !!this.ws &&
      (this.ws.readyState === WebSocket.OPEN ||
        this.ws.readyState === WebSocket.CONNECTING);

    this.origin = origin;
    this.manualClose = false;
    this.clearReconnectTimer();
    if (alreadyActive) {
      return;
    }

    this.connectionId += 1;
    this.teardownSocket();
    this.open(this.connectionId);
  }

  disconnect() {
    // manualClose stops the exponential backoff loop. Without it, normal flows
    // like logout, removeDevice or switching devices would reconnect instantly.
    this.manualClose = true;
    this.clearReconnectTimer();
    this.connectionId += 1;
    this.teardownSocket();
    this.handlers.onConnectionStateChange?.("closed");
  }

  private teardownSocket() {
    if (!this.ws) {
      return;
    }

    const ws = this.ws;
    this.ws = null;
    ws.onopen = null;
    ws.onmessage = null;
    ws.onerror = null;
    ws.onclose = null;

    if (
      ws.readyState === WebSocket.OPEN ||
      ws.readyState === WebSocket.CONNECTING
    ) {
      ws.close();
    }
  }

  private open(connectionId: number) {
    if (!this.origin) return;

    this.handlers.onConnectionStateChange?.(
      this.reconnectAttempt > 0 ? "reconnecting" : "connecting",
    );

    const ws = new WebSocket(buildDeviceWebSocketUrl(this.origin));
    ws.binaryType = "arraybuffer";

    ws.onopen = () => {
      if (connectionId !== this.connectionId) {
        ws.close();
        return;
      }

      this.reconnectAttempt = 0;
      this.handlers.onConnectionStateChange?.("connected");
      // We subscribe and request an immediate snapshot so the panel can render a
      // useful overview even before fresh live events start arriving.
      this.sendJson({ subscribe: "telemetry" });
      this.sendJson({ snapshot: "telemetry" });
      this.sendJson({ subscribe: "system_status" });
      this.sendJson({ snapshot: "system_status" });
      this.sendJson({ subscribe: "ble" });
      this.sendJson({ snapshot: "ble" });
      this.sendJson({ subscribe: "shelly" });
      this.sendJson({ snapshot: "shelly" });
    };

    ws.onmessage = (event) => {
      if (connectionId !== this.connectionId) {
        return;
      }

      if (event.data instanceof ArrayBuffer) {
        const telemetryEvent = parseSensorTelemetryPacket(event.data);
        if (telemetryEvent) {
          this.handlers.onTelemetryEvent?.(telemetryEvent);
          return;
        }

        const bleEvent = parseBleEventPacket(event.data);
        if (bleEvent) {
          this.handlers.onBleEvent?.(bleEvent);
          return;
        }

        const shellyEvent = parseShellyEventPacket(event.data);
        if (shellyEvent) {
          this.handlers.onShellyEvent?.(shellyEvent);
          return;
        }

        const packet = parseSystemStatusPacket(event.data);
        if (packet) {
          this.handlers.onSystemPacket?.(packet);
        }
        return;
      }

      if (typeof event.data !== "string") {
        return;
      }

      this.handleTextFrame(event.data);
    };

    ws.onerror = (event) => {
      if (connectionId !== this.connectionId) {
        return;
      }

      this.handlers.onError?.(event);
    };

    ws.onclose = () => {
      if (connectionId !== this.connectionId) {
        return;
      }

      if (this.ws === ws) {
        this.ws = null;
      }

      if (this.manualClose) {
        this.handlers.onConnectionStateChange?.("closed");
        return;
      }

      this.scheduleReconnect();
    };

    this.ws = ws;
  }

  private scheduleReconnect() {
    this.clearReconnectTimer();
    this.reconnectAttempt += 1;
    this.handlers.onConnectionStateChange?.("reconnecting");
    // Keep retries gentle for an embedded device. The cap avoids aggressive
    // reconnect storms when the ESP32 is rebooting or Wi-Fi is recovering.
    const delayMs = Math.min(
      1000 * 2 ** Math.max(0, this.reconnectAttempt - 1),
      15000,
    );
    const activeConnectionId = this.connectionId;
    this.reconnectTimer = setTimeout(
      () => this.open(activeConnectionId),
      delayMs,
    );
  }

  private clearReconnectTimer() {
    if (!this.reconnectTimer) return;
    clearTimeout(this.reconnectTimer);
    this.reconnectTimer = null;
  }

  private sendJson(payload: Record<string, string>) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      return;
    }
    this.ws.send(JSON.stringify(payload));
  }

  private handleTextFrame(frame: string) {
    const payload = parseRealtimeEnvelopeFrame(frame);
    if (!payload) {
      console.warn(
        "[DeviceOverviewSocket] Ignored malformed realtime JSON frame.",
      );
      return;
    }

    if (payload.type === "snapshot" && payload.channel === "telemetry") {
      const snapshot = parseTelemetrySnapshot(payload.data);
      if (snapshot) {
        this.handlers.onTelemetrySnapshot?.(snapshot);
      } else {
        console.warn(
          "[DeviceOverviewSocket] Ignored malformed telemetry snapshot.",
          payload.data,
        );
      }
      return;
    }

    if (payload.type === "snapshot" && payload.channel === "system_status") {
      const snapshot = parseSystemStatusSnapshot(payload.data);
      if (snapshot) {
        this.handlers.onSystemSnapshot?.(snapshot);
      } else {
        console.warn(
          "[DeviceOverviewSocket] Ignored malformed system snapshot.",
          payload.data,
        );
      }
      return;
    }

    if (payload.type === "snapshot" && payload.channel === "ble") {
      const snapshot = parseBleStatusSnapshot(payload.data);
      if (snapshot) {
        this.handlers.onBleSnapshot?.(snapshot);
      } else {
        console.warn(
          "[DeviceOverviewSocket] Ignored malformed BLE snapshot.",
          payload.data,
        );
      }
      return;
    }

    if (payload.type === "snapshot" && payload.channel === "shelly") {
      const snapshot = parseShellySnapshot(payload.data);
      if (snapshot) {
        this.handlers.onShellySnapshot?.(snapshot);
      } else {
        console.warn(
          "[DeviceOverviewSocket] Ignored malformed Shelly snapshot.",
          payload.data,
        );
      }
      return;
    }

    if (payload.type === "sensor") {
      const event = parseSensorTelemetryEvent(payload.data);
      if (event) {
        this.handlers.onTelemetryEvent?.(event);
      } else {
        console.warn(
          "[DeviceOverviewSocket] Ignored malformed telemetry event.",
          payload.data,
        );
      }
    }
  }
}
