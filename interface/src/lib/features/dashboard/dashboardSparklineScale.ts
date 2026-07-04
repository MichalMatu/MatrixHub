export interface DashboardSparklineDomain {
	min: number;
	max: number;
}

export interface DashboardSparklineScaleConfig {
	minSpan: number;
	paddingRatio?: number;
	floor?: number;
	ceil?: number;
	roundTo?: number;
}

export const DASHBOARD_SPARKLINE_SCALE_CONFIGS = {
	co2: {
		minSpan: 160,
		paddingRatio: 0.08,
		floor: 0,
		roundTo: 10
	},
	temp: {
		minSpan: 2,
		paddingRatio: 0.08,
		roundTo: 0.2
	},
	humid: {
		minSpan: 2,
		paddingRatio: 0.08,
		floor: 0,
		ceil: 100,
		roundTo: 0.2
	}
} satisfies Record<'co2' | 'temp' | 'humid', DashboardSparklineScaleConfig>;

function roundDown(value: number, step: number): number {
	return Math.floor(value / step) * step;
}

function roundUp(value: number, step: number): number {
	return Math.ceil(value / step) * step;
}

function shiftIntoBounds(
	min: number,
	max: number,
	floor?: number,
	ceil?: number
): DashboardSparklineDomain {
	let nextMin = min;
	let nextMax = max;

	if (floor != null && nextMin < floor) {
		const shift = floor - nextMin;
		nextMin += shift;
		nextMax += shift;
	}

	if (ceil != null && nextMax > ceil) {
		const shift = nextMax - ceil;
		nextMin -= shift;
		nextMax -= shift;
	}

	if (floor != null) {
		nextMin = Math.max(floor, nextMin);
	}

	if (ceil != null) {
		nextMax = Math.min(ceil, nextMax);
	}

	return { min: nextMin, max: nextMax };
}

export function resolveDashboardSparklineDomain(
	values: number[],
	config: DashboardSparklineScaleConfig
): DashboardSparklineDomain | null {
	const finiteValues = values.filter((value): value is number => Number.isFinite(value));

	if (finiteValues.length === 0) {
		return null;
	}

	const dataMin = Math.min(...finiteValues);
	const dataMax = Math.max(...finiteValues);
	const center = (dataMin + dataMax) / 2;
	const rawSpan = Math.max(0, dataMax - dataMin);
	const paddingRatio = config.paddingRatio ?? 0.08;
	const targetSpan = Math.max(rawSpan, config.minSpan);
	const desiredSpan = targetSpan * (1 + paddingRatio * 2);

	let domain = shiftIntoBounds(
		center - desiredSpan / 2,
		center + desiredSpan / 2,
		config.floor,
		config.ceil
	);

	if (config.roundTo && config.roundTo > 0) {
		domain = shiftIntoBounds(
			roundDown(domain.min, config.roundTo),
			roundUp(domain.max, config.roundTo),
			config.floor,
			config.ceil
		);
	}

	if (!(domain.max > domain.min)) {
		const fallbackSpan = Math.max(config.minSpan, config.roundTo ?? 0.1);
		domain = shiftIntoBounds(
			center - fallbackSpan / 2,
			center + fallbackSpan / 2,
			config.floor,
			config.ceil
		);
	}

	return domain.max > domain.min ? domain : null;
}
