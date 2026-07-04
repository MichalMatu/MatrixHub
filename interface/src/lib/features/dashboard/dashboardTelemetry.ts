export const DASHBOARD_HISTORY_POINTS = 48;

interface DashboardMetricHistory {
	values: number[];
	timestamps: number[];
}

export interface DashboardHistoryState {
	co2: DashboardMetricHistory;
	temp: DashboardMetricHistory;
	humid: DashboardMetricHistory;
}

export interface TelemetryHistorySnapshot {
	timestamps?: number[];
	co2?: Array<number | null>;
	temp?: Array<number | null>;
	humid?: Array<number | null>;
}

export const EMPTY_DASHBOARD_HISTORY_STATE: DashboardHistoryState = {
	co2: { values: [], timestamps: [] },
	temp: { values: [], timestamps: [] },
	humid: { values: [], timestamps: [] }
};

function trimTail<T>(values: T[], maxPoints: number): T[] {
	return values.slice(-maxPoints);
}

function isFiniteNumber(value: number | null | undefined): value is number {
	return value !== null && value !== undefined && Number.isFinite(value);
}

function normalizeMetricHistory(
	values: Array<number | null> | undefined,
	timestamps: number[] | undefined,
	maxPoints = DASHBOARD_HISTORY_POINTS
): DashboardMetricHistory {
	const safeValues = Array.isArray(values) ? values : [];
	const safeTimestamps = Array.isArray(timestamps) ? timestamps : [];

	if (safeTimestamps.length === 0) {
		return {
			values: trimTail(safeValues.filter(isFiniteNumber), maxPoints),
			timestamps: []
		};
	}

	const normalizedValues: number[] = [];
	const normalizedTimestamps: number[] = [];
	const count = Math.min(safeValues.length, safeTimestamps.length);

	for (let index = 0; index < count; index += 1) {
		const value = safeValues[index];
		const timestamp = safeTimestamps[index];
		if (!isFiniteNumber(value) || !Number.isFinite(timestamp)) {
			continue;
		}

		normalizedValues.push(value);
		normalizedTimestamps.push(timestamp);
	}

	return {
		values: trimTail(normalizedValues, maxPoints),
		timestamps: trimTail(normalizedTimestamps, maxPoints)
	};
}

export function buildDashboardHistoryState(
	historyData?: TelemetryHistorySnapshot,
	maxPoints = DASHBOARD_HISTORY_POINTS
): DashboardHistoryState {
	const timestamps = Array.isArray(historyData?.timestamps) ? historyData.timestamps : [];

	return {
		co2: normalizeMetricHistory(historyData?.co2, timestamps, maxPoints),
		temp: normalizeMetricHistory(historyData?.temp, timestamps, maxPoints),
		humid: normalizeMetricHistory(historyData?.humid, timestamps, maxPoints)
	};
}

export function appendDashboardHistoryPoint(
	history: DashboardHistoryState,
	point: { co2: number; temp: number; humid: number },
	timestamp: number,
	maxPoints = DASHBOARD_HISTORY_POINTS
): DashboardHistoryState {
	return {
		co2: {
			values: trimTail([...history.co2.values, point.co2], maxPoints),
			timestamps: trimTail([...history.co2.timestamps, timestamp], maxPoints)
		},
		temp: {
			values: trimTail([...history.temp.values, point.temp], maxPoints),
			timestamps: trimTail([...history.temp.timestamps, timestamp], maxPoints)
		},
		humid: {
			values: trimTail([...history.humid.values, point.humid], maxPoints),
			timestamps: trimTail([...history.humid.timestamps, timestamp], maxPoints)
		}
	};
}
