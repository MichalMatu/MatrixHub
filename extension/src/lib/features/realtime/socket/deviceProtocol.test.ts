import { describe, expect, it } from "vitest";
import {
  parseShellyEventPacket,
  parseShellySnapshot,
  parseRealtimeEnvelopeFrame,
  parseSensorTelemetryEvent,
  parseSystemStatusSnapshot,
  parseTelemetrySnapshot,
} from "@matrixhub/device-sdk";

describe("device realtime protocol parsers", () => {
  it("parses telemetry snapshots and keeps valid history arrays", () => {
    expect(
      parseTelemetrySnapshot({
        co2: 500,
        temp: 22.5,
        humid: 48.2,
        lastReadOk: true,
        history: {
          timestamps: [1, 2],
          co2: [400, 500],
          temp: [20, 22.5],
          humid: [45, 48.2],
        },
      }),
    ).toEqual({
      co2: 500,
      temp: 22.5,
      humid: 48.2,
      lastReadOk: true,
      history: {
        timestamps: [1, 2],
        co2: [400, 500],
        temp: [20, 22.5],
        humid: [45, 48.2],
      },
    });
  });

  it("rejects malformed sensor telemetry events", () => {
    expect(
      parseSensorTelemetryEvent({
        co2: 500,
        temp: 22.5,
        humid: 48.2,
        lastReadOk: "yes",
        timestamp_ms: 1,
      }),
    ).toBeNull();
  });

  it("sanitizes system status snapshots down to known diagnostics", () => {
    expect(
      parseSystemStatusSnapshot({
        diagnostics: {
          wifi: {
            healthy: true,
            state: "sta_connected",
            unknown: "skip",
          },
        },
      }),
    ).toEqual({
      diagnostics: {
        wifi: {
          healthy: true,
          state: "sta_connected",
        },
      },
    });
  });

  it("parses Shelly snapshots and keeps supported fields", () => {
    expect(
      parseShellySnapshot([
        {
          id: "relay-alpha-01",
          name: "Desk Lamp",
          isOn: true,
          isOnline: true,
          relay_index: 1,
          power: 12.5,
        },
        {
          id: "broken",
        },
      ]),
    ).toEqual([
      {
        id: "relay-alpha-01",
        name: "Desk Lamp",
        isOn: true,
        isOnline: true,
        relayIndex: 1,
        power: 12.5,
      },
    ]);
  });

  it("parses binary Shelly events", () => {
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
    view.setUint16(offset, 1234, true);
    offset += 2;
    view.setUint16(offset, 2301, true);
    offset += 2;
    view.setUint16(offset, 456, true);
    offset += 2;
    view.setUint32(offset, 789, true);
    offset += 4;
    view.setInt8(offset++, 26);
    view.setInt8(offset, -61);

    expect(parseShellyEventPacket(buffer)).toEqual({
      id: "relay-al",
      isOnline: true,
      isOn: true,
      power: 12.34,
      voltage: 230.1,
      current: 0.456,
      energy: 78.9,
      temp: 26,
      rssi: -61,
    });
  });

  it("parses text envelopes and rejects invalid json", () => {
    expect(
      parseRealtimeEnvelopeFrame(
        JSON.stringify({
          type: "snapshot",
          channel: "telemetry",
          data: {
            co2: 500,
          },
        }),
      ),
    ).toEqual({
      type: "snapshot",
      channel: "telemetry",
      data: {
        co2: 500,
      },
    });
    expect(parseRealtimeEnvelopeFrame("{bad json")).toBeNull();
  });
});
