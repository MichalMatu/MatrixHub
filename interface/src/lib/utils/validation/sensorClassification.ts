/**
 * Sensor classification utilities
 * Provides statistics calculation for sensor data
 */

export interface Stats {
	min: number;
	max: number;
	avg: number;
}

// Calculate statistics from history data
export function calculateStats(data: number[]): Stats {
	if (data.length === 0) return { min: 0, max: 0, avg: 0 };

	const min = Math.min(...data);
	const max = Math.max(...data);
	const avg = data.reduce((a, b) => a + b, 0) / data.length;

	return { min, max, avg };
}
