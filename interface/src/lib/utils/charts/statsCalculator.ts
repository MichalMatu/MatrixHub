/**
 * Chart statistics calculator
 */

export interface ChartStats {
	min: number;
	max: number;
	avg: number;
}

/**
 * Calculate statistics (min, max, avg) for chart data
 */
export function calcStats(data: (number | null)[]): ChartStats | null {
	const valid = data.filter((v): v is number => v !== null && !isNaN(v));
	if (valid.length === 0) return null;

	const min = Math.min(...valid);
	const max = Math.max(...valid);
	const avg = valid.reduce((a, b) => a + b, 0) / valid.length;

	return { min, max, avg };
}
