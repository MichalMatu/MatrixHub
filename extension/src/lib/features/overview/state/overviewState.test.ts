import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import {
  applyRealtimeTelemetryEvent,
  createEmptyOverviewState,
} from "./overviewState";

describe("overviewState", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date("2026-01-01T12:00:00.000Z"));
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("uses receive time when telemetry event timestamp is device uptime", () => {
    const nextState = applyRealtimeTelemetryEvent(
      createEmptyOverviewState(),
      {
        co2: 612,
        temp: 23.4,
        humid: 56.1,
        lastReadOk: true,
        timestamp_ms: 1_200_000,
      },
      Date.parse("2026-01-01T12:00:00.000Z"),
    );

    expect(nextState.lastTelemetryAt).toBe("2026-01-01T12:00:00.000Z");
  });

  it("keeps absolute wall-clock timestamps when they are provided", () => {
    const nextState = applyRealtimeTelemetryEvent(
      createEmptyOverviewState(),
      {
        co2: 612,
        temp: 23.4,
        humid: 56.1,
        lastReadOk: true,
        timestamp_ms: Date.parse("2026-01-01T11:59:30.000Z"),
      },
      Date.parse("2026-01-01T12:00:00.000Z"),
    );

    expect(nextState.lastTelemetryAt).toBe("2026-01-01T11:59:30.000Z");
  });

  it("appends live telemetry points to the sparkline history", () => {
    const now = Date.parse("2026-01-01T12:00:00.000Z");
    const nowSeconds = Math.floor(now / 1000);

    const nextState = applyRealtimeTelemetryEvent(
      {
        ...createEmptyOverviewState(),
        telemetrySnapshot: {
          co2: 606,
          temp: 22.2,
          humid: 55.5,
          lastReadOk: true,
          history: {
            timestamps: [nowSeconds - 2, nowSeconds - 1],
            co2: [600, 606],
            temp: [22.0, 22.2],
            humid: [55.0, 55.5],
          },
        },
      },
      {
        co2: 612,
        temp: 23.4,
        humid: 56.1,
        lastReadOk: true,
        timestamp_ms: 1_200_000,
      },
      now,
    );

    expect(nextState.telemetrySnapshot?.history).toEqual({
      timestamps: [nowSeconds - 2, nowSeconds - 1, nowSeconds],
      co2: [600, 606, 612],
      temp: [22.0, 22.2, 23.4],
      humid: [55.0, 55.5, 56.1],
    });
  });
});
