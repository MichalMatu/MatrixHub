export type EnvironmentSensorKey = 'co2' | 'temperature' | 'humidity';

export interface EnvironmentSeriesInput {
	key: EnvironmentSensorKey;
	label: string;
	color: string;
	unit: string;
	decimals: number;
	data: (number | null)[];
	minPlotRange: number;
	band: EnvironmentPlotBand;
}

export interface EnvironmentSeriesStats {
	min: number;
	max: number;
	avg: number;
	first: number;
	latest: number;
	latestIndex: number;
	delta: number;
	range: number;
}

export interface EnvironmentSeriesModel extends EnvironmentSeriesInput {
	bandData: (number | null)[];
	stats: EnvironmentSeriesStats | null;
	plotRange: [number, number] | null;
}

export interface EnvironmentSampleValue {
	key: EnvironmentSensorKey;
	label: string;
	color: string;
	unit: string;
	decimals: number;
	value: number | null;
}

export interface EnvironmentPlotBand {
	min: number;
	max: number;
}

export interface EnvironmentPlotData {
	timestamps: number[];
	rawIndexes: (number | null)[];
	series: Record<EnvironmentSensorKey, (number | null)[]>;
}

function isFiniteNumber(value: number | null | undefined): value is number {
	return typeof value === 'number' && Number.isFinite(value);
}

function clamp(value: number, min: number, max: number) {
	return Math.min(max, Math.max(min, value));
}

export function calculateEnvironmentSeriesStats(
	data: (number | null)[]
): EnvironmentSeriesStats | null {
	const valid: { value: number; index: number }[] = [];

	data.forEach((value, index) => {
		if (isFiniteNumber(value)) {
			valid.push({ value, index });
		}
	});

	if (valid.length === 0) return null;

	const values = valid.map((entry) => entry.value);
	const min = Math.min(...values);
	const max = Math.max(...values);
	const first = valid[0].value;
	const latestEntry = valid[valid.length - 1];
	const latest = latestEntry.value;
	const avg = values.reduce((sum, value) => sum + value, 0) / values.length;

	return {
		min,
		max,
		avg,
		first,
		latest,
		latestIndex: latestEntry.index,
		delta: latest - first,
		range: max - min
	};
}

export function getEnvironmentPlotRange(
	stats: EnvironmentSeriesStats | null,
	minPlotRange: number
): [number, number] | null {
	if (!stats) return null;

	const range = Math.max(stats.range, minPlotRange);
	const pad = Math.max(range * 0.08, minPlotRange * 0.04);
	const center = (stats.min + stats.max) / 2;
	const half = range / 2 + pad;

	return [center - half, center + half];
}

export function projectEnvironmentBand(
	data: (number | null)[],
	minPlotRange: number,
	band: EnvironmentPlotBand
): (number | null)[] {
	const stats = calculateEnvironmentSeriesStats(data);
	if (!stats) return data.map(() => null);

	const plotRange = getEnvironmentPlotRange(stats, minPlotRange);
	if (!plotRange) return data.map(() => null);

	const [lo, hi] = plotRange;
	const span = hi - lo;
	const bandSpan = band.max - band.min;

	return data.map((value) => {
		if (!isFiniteNumber(value)) return null;
		const ratio = span === 0 ? 0.5 : (value - lo) / span;
		return clamp(band.min + ratio * bandSpan, band.min, band.max);
	});
}

export function buildEnvironmentSeriesModels(
	series: EnvironmentSeriesInput[]
): EnvironmentSeriesModel[] {
	return series.map((input) => {
		const stats = calculateEnvironmentSeriesStats(input.data);
		return {
			...input,
			stats,
			plotRange: getEnvironmentPlotRange(stats, input.minPlotRange),
			bandData: projectEnvironmentBand(input.data, input.minPlotRange, input.band)
		};
	});
}

export function buildEnvironmentPlotData(
	timestamps: number[],
	series: EnvironmentSeriesModel[],
	gapThresholdSeconds = 60 * 60
): EnvironmentPlotData {
	const plotData: EnvironmentPlotData = {
		timestamps: [],
		rawIndexes: [],
		series: {
			co2: [],
			temperature: [],
			humidity: []
		}
	};

	for (let index = 0; index < timestamps.length; index++) {
		const previousTimestamp = timestamps[index - 1];
		const timestamp = timestamps[index];

		if (
			index > 0 &&
			Number.isFinite(previousTimestamp) &&
			Number.isFinite(timestamp) &&
			timestamp - previousTimestamp > gapThresholdSeconds
		) {
			plotData.timestamps.push(previousTimestamp + (timestamp - previousTimestamp) / 2);
			plotData.rawIndexes.push(null);
			for (const key of Object.keys(plotData.series) as EnvironmentSensorKey[]) {
				plotData.series[key].push(null);
			}
		}

		plotData.timestamps.push(timestamp);
		plotData.rawIndexes.push(index);
		for (const key of Object.keys(plotData.series) as EnvironmentSensorKey[]) {
			plotData.series[key].push(null);
		}
		for (const model of series) {
			plotData.series[model.key][plotData.series[model.key].length - 1] =
				model.bandData[index] ?? null;
		}
	}

	return plotData;
}

export function getEnvironmentSampleValues(
	series: EnvironmentSeriesModel[],
	index: number | null
): EnvironmentSampleValue[] {
	return series.map(({ key, label, color, unit, decimals, data }) => ({
		key,
		label,
		color,
		unit,
		decimals,
		value: index == null ? null : (data[index] ?? null)
	}));
}

export function formatSensorValue(
	value: number | null | undefined,
	decimals: number,
	unit: string
): string {
	if (!isFiniteNumber(value)) return '-';
	return `${value.toFixed(decimals)}${unit}`;
}
