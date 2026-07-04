import type {
  TelemetryHistorySnapshot,
  TelemetrySnapshot,
} from "@matrixhub/device-sdk";
import {
  formatCo2Value,
  formatHumidityValue,
  formatTemperatureValue,
} from "$lib/i18n/formatters";
import { getI18nRuntime, type I18nRuntime } from "$lib/i18n/runtime";
import { normalizeMetricHistory } from "$lib/features/overview/telemetryHistory";
import type { OverviewState } from "$lib/features/overview/state/overviewState";
import type {
  DetailItem,
  OverviewCardViewModel,
  OverviewMetricId,
  OverviewMetricViewModel,
} from "./viewModelTypes";

type OverviewTelemetryLabelKey =
  | "overview.telemetry.co2"
  | "overview.telemetry.temperature"
  | "overview.telemetry.humidity";

interface TelemetryDefinition {
  id: OverviewMetricId;
  labelKey: OverviewTelemetryLabelKey;
  chartColor: string;
  readValue: (snapshot: TelemetrySnapshot) => number;
  readHistory: (
    history: TelemetryHistorySnapshot | undefined,
  ) => Array<number | null> | undefined;
  format: (i18n: I18nRuntime, snapshot: TelemetrySnapshot) => string;
  domainMin?: number;
  domainMax?: number;
}

const telemetryDefinitions: TelemetryDefinition[] = [
  {
    id: "co2",
    labelKey: "overview.telemetry.co2",
    chartColor: "#22c55e",
    readValue: (snapshot: TelemetrySnapshot) => snapshot.co2,
    readHistory: (history: TelemetryHistorySnapshot | undefined) =>
      history?.co2,
    format: (i18n: I18nRuntime, snapshot: TelemetrySnapshot) =>
      formatCo2Value(i18n, snapshot.co2),
  },
  {
    id: "temperature",
    labelKey: "overview.telemetry.temperature",
    chartColor: "#ef4444",
    readValue: (snapshot: TelemetrySnapshot) => snapshot.temp,
    readHistory: (history: TelemetryHistorySnapshot | undefined) =>
      history?.temp,
    format: (i18n: I18nRuntime, snapshot: TelemetrySnapshot) =>
      formatTemperatureValue(i18n, snapshot.temp),
  },
  {
    id: "humidity",
    labelKey: "overview.telemetry.humidity",
    chartColor: "#3b82f6",
    readValue: (snapshot: TelemetrySnapshot) => snapshot.humid,
    readHistory: (history: TelemetryHistorySnapshot | undefined) =>
      history?.humid,
    domainMin: 0,
    domainMax: 100,
    format: (i18n: I18nRuntime, snapshot: TelemetrySnapshot) =>
      formatHumidityValue(i18n, snapshot.humid),
  },
];

function formatTelemetryValues(
  snapshot: TelemetrySnapshot,
  i18n: I18nRuntime,
): DetailItem[] {
  return telemetryDefinitions.map((definition) => ({
    id: definition.id,
    label: i18n.t(definition.labelKey),
    value: definition.format(i18n, snapshot),
  }));
}

function buildOverviewMetrics(
  snapshot: TelemetrySnapshot,
  i18n: I18nRuntime,
): OverviewMetricViewModel[] {
  const timestamps = snapshot.history?.timestamps;

  return telemetryDefinitions.map((definition) => {
    const history = normalizeMetricHistory(
      definition.readHistory(snapshot.history),
      timestamps,
    );

    return {
      id: definition.id,
      label: i18n.t(definition.labelKey),
      value: definition.readValue(snapshot),
      history: history.values,
      timestamps: history.timestamps,
      chartColor: definition.chartColor,
      ...(definition.domainMin !== undefined
        ? { domainMin: definition.domainMin }
        : {}),
      ...(definition.domainMax !== undefined
        ? { domainMax: definition.domainMax }
        : {}),
    };
  });
}

export function buildOverviewCardViewModel(input: {
  state: OverviewState;
  i18n?: I18nRuntime;
}): OverviewCardViewModel {
  const i18n = input.i18n ?? getI18nRuntime();
  const snapshot = input.state.telemetrySnapshot;

  return {
    details: snapshot ? formatTelemetryValues(snapshot, i18n) : [],
    metrics: snapshot ? buildOverviewMetrics(snapshot, i18n) : [],
  };
}
