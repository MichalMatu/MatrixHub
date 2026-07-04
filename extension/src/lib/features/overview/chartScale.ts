import type { OverviewMetricViewModel } from "./mappers/viewModels";

const MIN_VISUAL_SPAN: Record<OverviewMetricViewModel["id"], number> = {
  co2: 20,
  temperature: 0.5,
  humidity: 2,
};

export interface MetricChartScale {
  min: number;
  max: number;
}

function roundToPrecision(value: number, fractionDigits = 6) {
  return Number(value.toFixed(fractionDigits));
}

function collectMetricValues(metric: OverviewMetricViewModel) {
  const values = metric.history.length > 0 ? metric.history : [metric.value];
  return values.filter((value) => Number.isFinite(value));
}

export function buildMetricChartScale(
  metric: OverviewMetricViewModel,
): MetricChartScale {
  const values = collectMetricValues(metric);
  const actualMin = Math.min(...values);
  const actualMax = Math.max(...values);
  const actualRange = Math.max(0, actualMax - actualMin);
  const paddedRange =
    actualRange > 0 ? actualRange * 1.2 : MIN_VISUAL_SPAN[metric.id];
  const desiredRange = Math.max(paddedRange, MIN_VISUAL_SPAN[metric.id]);
  const boundarySnapThreshold = desiredRange * 0.1;
  const center = (actualMin + actualMax) / 2;
  let min = center - desiredRange / 2;
  let max = center + desiredRange / 2;

  if (
    metric.domainMin !== undefined &&
    min <= metric.domainMin + boundarySnapThreshold
  ) {
    min = metric.domainMin;
    max = min + desiredRange;
  }

  if (
    metric.domainMax !== undefined &&
    max >= metric.domainMax - boundarySnapThreshold
  ) {
    max = metric.domainMax;
    min = max - desiredRange;
  }

  if (metric.domainMin !== undefined && min < metric.domainMin) {
    const shift = metric.domainMin - min;
    min = metric.domainMin;
    max += shift;
  }

  if (metric.domainMax !== undefined && max > metric.domainMax) {
    const shift = max - metric.domainMax;
    max = metric.domainMax;
    min -= shift;
  }

  if (metric.domainMin !== undefined) {
    min = Math.max(metric.domainMin, min);
  }

  if (metric.domainMax !== undefined) {
    max = Math.min(metric.domainMax, max);
  }

  if (!Number.isFinite(min) || !Number.isFinite(max) || max <= min) {
    return {
      min: roundToPrecision(actualMin - 1),
      max: roundToPrecision(actualMax + 1),
    };
  }

  return {
    min: roundToPrecision(min),
    max: roundToPrecision(max),
  };
}
