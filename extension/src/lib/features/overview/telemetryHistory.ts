import type {
  TelemetryHistorySnapshot,
  TelemetrySnapshot,
} from "@matrixhub/device-sdk";

export const OVERVIEW_HISTORY_POINTS = 48;

const MIN_VALID_ABSOLUTE_TIMESTAMP_MS = Date.UTC(2000, 0, 1);
const MILLISECOND_TIMESTAMP_THRESHOLD = 100_000_000_000;

export interface NormalizedMetricHistory {
  values: number[];
  timestamps: number[];
}

function trimTail<T>(values: T[], maxPoints: number) {
  return values.slice(-maxPoints);
}

function isFiniteNumber(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value);
}

function normalizeSeries(
  values: Array<number | null> | undefined,
  startIndex: number,
  count: number,
) {
  const safeValues = Array.isArray(values) ? values : [];

  return Array.from({ length: count }, (_, offset) => {
    const value = safeValues[startIndex + offset];
    return value === null || isFiniteNumber(value) ? value : null;
  });
}

export function normalizeTelemetryHistorySnapshot(
  history: TelemetryHistorySnapshot | undefined,
  maxPoints = OVERVIEW_HISTORY_POINTS,
): TelemetryHistorySnapshot | undefined {
  if (!history) {
    return undefined;
  }

  const safeTimestamps = Array.isArray(history.timestamps)
    ? history.timestamps.filter(isFiniteNumber)
    : [];

  if (safeTimestamps.length === 0) {
    return undefined;
  }

  const startIndex = Math.max(0, safeTimestamps.length - maxPoints);
  const timestamps = safeTimestamps.slice(startIndex);

  return {
    timestamps,
    co2: normalizeSeries(history.co2, startIndex, timestamps.length),
    temp: normalizeSeries(history.temp, startIndex, timestamps.length),
    humid: normalizeSeries(history.humid, startIndex, timestamps.length),
  };
}

export function normalizeMetricHistory(
  values: Array<number | null> | undefined,
  timestamps: number[] | undefined,
  maxPoints = OVERVIEW_HISTORY_POINTS,
): NormalizedMetricHistory {
  const safeValues = Array.isArray(values) ? values : [];
  const safeTimestamps = Array.isArray(timestamps) ? timestamps : [];

  if (safeTimestamps.length === 0) {
    return {
      values: trimTail(
        safeValues.filter((value): value is number => isFiniteNumber(value)),
        maxPoints,
      ),
      timestamps: [],
    };
  }

  const count = Math.min(safeValues.length, safeTimestamps.length);
  const normalizedValues: number[] = [];
  const normalizedTimestamps: number[] = [];

  for (let index = 0; index < count; index += 1) {
    const value = safeValues[index];
    const timestamp = safeTimestamps[index];

    if (!isFiniteNumber(value) || !isFiniteNumber(timestamp)) {
      continue;
    }

    normalizedValues.push(value);
    normalizedTimestamps.push(timestamp);
  }

  return {
    values: trimTail(normalizedValues, maxPoints),
    timestamps: trimTail(normalizedTimestamps, maxPoints),
  };
}

function usesMillisecondTimestamps(timestamps: number[] | undefined) {
  const lastTimestamp = timestamps?.[timestamps.length - 1];
  return (
    typeof lastTimestamp === "number" &&
    lastTimestamp >= MILLISECOND_TIMESTAMP_THRESHOLD
  );
}

export function resolveHistoryTimestamp(input: {
  timestampMs: number | null | undefined;
  timestamps?: number[];
  fallbackNow?: number;
}) {
  const { timestampMs, timestamps = [], fallbackNow = Date.now() } = input;
  const useMilliseconds = usesMillisecondTimestamps(timestamps);
  const baseFallback = useMilliseconds
    ? fallbackNow
    : Math.floor(fallbackNow / 1000);
  const increment = useMilliseconds ? 1000 : 1;
  const lastTimestamp = timestamps[timestamps.length - 1];

  if (
    isFiniteNumber(timestampMs) &&
    timestampMs >= MIN_VALID_ABSOLUTE_TIMESTAMP_MS
  ) {
    return useMilliseconds ? timestampMs : Math.floor(timestampMs / 1000);
  }

  if (isFiniteNumber(lastTimestamp)) {
    return Math.max(baseFallback, lastTimestamp + increment);
  }

  return baseFallback;
}

export function appendTelemetryHistoryPoint(
  history: TelemetryHistorySnapshot | undefined,
  point: { co2: number; temp: number; humid: number },
  options: {
    timestampMs?: number | null;
    fallbackNow?: number;
    maxPoints?: number;
  } = {},
): TelemetryHistorySnapshot {
  const maxPoints = options.maxPoints ?? OVERVIEW_HISTORY_POINTS;
  const normalizedHistory = normalizeTelemetryHistorySnapshot(
    history,
    maxPoints,
  );
  const timestamps = normalizedHistory?.timestamps ?? [];

  return {
    timestamps: trimTail(
      [
        ...timestamps,
        resolveHistoryTimestamp({
          timestampMs: options.timestampMs,
          timestamps,
          fallbackNow: options.fallbackNow,
        }),
      ],
      maxPoints,
    ),
    co2: trimTail([...(normalizedHistory?.co2 ?? []), point.co2], maxPoints),
    temp: trimTail([...(normalizedHistory?.temp ?? []), point.temp], maxPoints),
    humid: trimTail(
      [...(normalizedHistory?.humid ?? []), point.humid],
      maxPoints,
    ),
  };
}

export function normalizeTelemetrySnapshot(
  snapshot: TelemetrySnapshot,
  maxPoints = OVERVIEW_HISTORY_POINTS,
): TelemetrySnapshot {
  const normalizedHistory = normalizeTelemetryHistorySnapshot(
    snapshot.history,
    maxPoints,
  );
  const nextSnapshot: TelemetrySnapshot = {
    ...snapshot,
  };

  if (normalizedHistory) {
    nextSnapshot.history = normalizedHistory;
    return nextSnapshot;
  }

  delete nextSnapshot.history;
  return nextSnapshot;
}
