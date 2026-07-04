import { describe, expect, it } from "vitest";
import { buildMetricChartScale } from "./chartScale";
import type { OverviewMetricViewModel } from "./mappers/viewModels";

function createMetric(
  overrides: Partial<OverviewMetricViewModel>,
): OverviewMetricViewModel {
  return {
    id: "temperature",
    label: "Temp",
    value: 23.2,
    history: [23.1, 23.2, 23.3],
    timestamps: [100, 200, 300],
    chartColor: "#ef4444",
    ...overrides,
  };
}

describe("chartScale", () => {
  it("keeps temperature changes readable without stretching tiny deltas edge to edge", () => {
    const scale = buildMetricChartScale(
      createMetric({
        id: "temperature",
        value: 23.3,
        history: [23.1, 23.2, 23.3],
      }),
    );

    expect(scale.max - scale.min).toBeGreaterThanOrEqual(0.5);
    expect(scale.min).toBeLessThan(23.1);
    expect(scale.max).toBeGreaterThan(23.3);
    expect(scale.max - scale.min).toBeLessThanOrEqual(0.6);
  });

  it("does not flatten humidity to a 0-100 axis when values move in a narrow band", () => {
    const scale = buildMetricChartScale(
      createMetric({
        id: "humidity",
        label: "Humidity",
        value: 55.2,
        history: [55.0, 55.1, 55.2],
        chartColor: "#3b82f6",
        domainMin: 0,
        domainMax: 100,
      }),
    );

    expect(scale.max - scale.min).toBeGreaterThanOrEqual(2);
    expect(scale.min).toBeGreaterThan(0);
    expect(scale.max).toBeLessThan(100);
    expect(scale.max - scale.min).toBeLessThanOrEqual(2.5);
  });

  it("clamps the scale when humidity trends near the lower boundary", () => {
    const scale = buildMetricChartScale(
      createMetric({
        id: "humidity",
        label: "Humidity",
        value: 1.2,
        history: [0.9, 1.0, 1.2],
        chartColor: "#3b82f6",
        domainMin: 0,
        domainMax: 100,
      }),
    );

    expect(scale.min).toBe(0);
    expect(scale.max).toBeGreaterThan(1.2);
  });
});
