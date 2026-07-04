import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { DeviceOverviewSocket } from "./deviceOverviewSocket";

class FakeWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;
  static instances: FakeWebSocket[] = [];

  readonly url: string;
  readyState = FakeWebSocket.CONNECTING;
  binaryType = "blob";
  sent: string[] = [];
  onopen: ((event: Event) => void) | null = null;
  onmessage: ((event: MessageEvent) => void) | null = null;
  onerror: ((event: Event) => void) | null = null;
  onclose: ((event: CloseEvent) => void) | null = null;

  constructor(url: string) {
    this.url = url;
    FakeWebSocket.instances.push(this);
  }

  static reset() {
    FakeWebSocket.instances = [];
  }

  send(data: string) {
    this.sent.push(data);
  }

  close() {
    this.readyState = FakeWebSocket.CLOSED;
    this.onclose?.({} as CloseEvent);
  }

  triggerOpen() {
    this.readyState = FakeWebSocket.OPEN;
    this.onopen?.({} as Event);
  }

  triggerMessage(data: unknown) {
    this.onmessage?.({ data } as MessageEvent);
  }
}

describe("DeviceOverviewSocket", () => {
  beforeEach(() => {
    FakeWebSocket.reset();
    vi.stubGlobal("WebSocket", FakeWebSocket);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    vi.restoreAllMocks();
    vi.useRealTimers();
  });

  it("does not open duplicate sockets for the same active origin", () => {
    const socket = new DeviceOverviewSocket({});

    socket.connect("https://matrixhub.local");
    socket.connect("https://matrixhub.local");

    expect(FakeWebSocket.instances).toHaveLength(1);
  });

  it("subscribes to telemetry and system snapshots after opening", () => {
    const socket = new DeviceOverviewSocket({});

    socket.connect("https://matrixhub.local");
    const ws = FakeWebSocket.instances[0];

    expect(ws).toBeDefined();

    ws!.triggerOpen();

    expect(ws!.sent).toEqual([
      JSON.stringify({ subscribe: "telemetry" }),
      JSON.stringify({ snapshot: "telemetry" }),
      JSON.stringify({ subscribe: "system_status" }),
      JSON.stringify({ snapshot: "system_status" }),
      JSON.stringify({ subscribe: "ble" }),
      JSON.stringify({ snapshot: "ble" }),
      JSON.stringify({ subscribe: "shelly" }),
      JSON.stringify({ snapshot: "shelly" }),
    ]);
  });

  it("forwards valid Shelly snapshots and binary events", () => {
    const onShellySnapshot = vi.fn();
    const onShellyEvent = vi.fn();
    const socket = new DeviceOverviewSocket({
      onShellySnapshot,
      onShellyEvent,
    });

    socket.connect("https://matrixhub.local");
    const ws = FakeWebSocket.instances[0];

    ws!.triggerMessage(
      JSON.stringify({
        type: "snapshot",
        channel: "shelly",
        data: [
          {
            id: "relay-alpha-01",
            name: "Desk Lamp",
            isOn: true,
            isOnline: true,
          },
        ],
      }),
    );

    const id = "relay-al";
    const buffer = new ArrayBuffer(23);
    const view = new DataView(buffer);
    const bytes = new Uint8Array(buffer);
    bytes[0] = 0x53;
    bytes[1] = id.length;
    Array.from(id).forEach((char, index) => {
      bytes[2 + index] = char.charCodeAt(0);
    });

    let offset = 2 + id.length;
    bytes[offset++] = 0x03;
    view.setUint16(offset, 500, true);
    offset += 2;
    view.setUint16(offset, 2300, true);
    offset += 2;
    view.setUint16(offset, 250, true);
    offset += 2;
    view.setUint32(offset, 120, true);
    offset += 4;
    view.setInt8(offset++, 25);
    view.setInt8(offset, -58);

    ws!.triggerMessage(buffer);

    expect(onShellySnapshot).toHaveBeenCalledWith([
      {
        id: "relay-alpha-01",
        name: "Desk Lamp",
        isOn: true,
        isOnline: true,
      },
    ]);
    expect(onShellyEvent).toHaveBeenCalledWith({
      id: "relay-al",
      isOnline: true,
      isOn: true,
      power: 5,
      voltage: 230,
      current: 0.25,
      energy: 12,
      temp: 25,
      rssi: -58,
    });
  });

  it("forwards binary telemetry packets from firmware", () => {
    const onTelemetryEvent = vi.fn();
    const socket = new DeviceOverviewSocket({
      onTelemetryEvent,
    });

    socket.connect("https://matrixhub.local");
    const ws = FakeWebSocket.instances[0];

    const buffer = new ArrayBuffer(12);
    const view = new DataView(buffer);
    view.setUint8(0, 0x54);
    view.setUint16(1, 768, true);
    view.setInt16(3, 269, true);
    view.setUint16(5, 400, true);
    view.setUint32(7, 16349124, true);
    view.setUint8(11, 0x01);

    ws!.triggerMessage(buffer);

    expect(onTelemetryEvent).toHaveBeenCalledWith({
      co2: 768,
      temp: 26.9,
      humid: 40,
      timestamp_ms: 16349124,
      lastReadOk: true,
    });
  });

  it("ignores malformed telemetry snapshots and keeps valid system snapshots", () => {
    const onTelemetrySnapshot = vi.fn();
    const onSystemSnapshot = vi.fn();
    const warnSpy = vi
      .spyOn(console, "warn")
      .mockImplementation(() => undefined);
    const socket = new DeviceOverviewSocket({
      onTelemetrySnapshot,
      onSystemSnapshot,
    });

    socket.connect("https://matrixhub.local");
    const ws = FakeWebSocket.instances[0];

    ws!.triggerMessage(
      JSON.stringify({
        type: "snapshot",
        channel: "telemetry",
        data: { co2: "bad", temp: 22, humid: 55 },
      }),
    );
    ws!.triggerMessage(
      JSON.stringify({
        type: "snapshot",
        channel: "system_status",
        data: { system_info: { firmware_version: "2.0.0" } },
      }),
    );

    expect(onTelemetrySnapshot).not.toHaveBeenCalled();
    expect(onSystemSnapshot).toHaveBeenCalledWith({
      system_info: { firmware_version: "2.0.0" },
    });
    expect(warnSpy).toHaveBeenCalled();
  });

  it("reconnects with backoff after an unexpected close", () => {
    vi.useFakeTimers();
    const states: string[] = [];
    const socket = new DeviceOverviewSocket({
      onConnectionStateChange: (state) => {
        states.push(state);
      },
    });

    socket.connect("https://matrixhub.local");
    const first = FakeWebSocket.instances[0];
    first!.triggerOpen();
    first!.close();

    vi.advanceTimersByTime(1000);

    expect(FakeWebSocket.instances).toHaveLength(2);
    expect(states).toContain("reconnecting");
  });
});
