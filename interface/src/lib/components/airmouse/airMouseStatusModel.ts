const TAP_FORCE_RANGE_G = 4;
const PERCENT_MAX = 100;
const HISTORY_MIN_POINTS = 2;
const HISTORY_FALLBACK_POINTS = 99;
const CHART_Y_CLAMP = 48;
const CHART_Y_DIVISOR = 2;

function toFiniteNumber(value: number | null | undefined, fallback = 0): number {
	return typeof value === 'number' && Number.isFinite(value) ? value : fallback;
}

export function toAirMousePercent(
	value: number | null | undefined,
	rangeMax: number = TAP_FORCE_RANGE_G
): number {
	const safeRange = toFiniteNumber(rangeMax, TAP_FORCE_RANGE_G);
	if (safeRange <= 0) return 0;

	const safeValue = Math.max(0, toFiniteNumber(value, 0));
	return Math.min((safeValue / safeRange) * PERCENT_MAX, PERCENT_MAX);
}

export function createAirMouseGyroPolylinePoints(history: number[], maxHistory: number): string {
	if (!history.length) return '';

	const safeMaxHistory =
		Number.isFinite(maxHistory) && maxHistory >= HISTORY_MIN_POINTS
			? maxHistory
			: HISTORY_FALLBACK_POINTS + 1;
	const maxPoints = safeMaxHistory - 1;
	const visibleHistory = history.slice(-safeMaxHistory);
	const offset = safeMaxHistory - visibleHistory.length;

	return visibleHistory
		.map((rawValue, index) => {
			const x = ((offset + index) / maxPoints) * PERCENT_MAX;
			const value = toFiniteNumber(rawValue, 0);
			const y = -Math.max(-CHART_Y_CLAMP, Math.min(CHART_Y_CLAMP, value / CHART_Y_DIVISOR));
			return `${x.toFixed(1)},${y.toFixed(1)}`;
		})
		.join(' ');
}
