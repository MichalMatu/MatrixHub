/**
 * Chart time formatters for uPlot
 */
import type uPlot from 'uplot';

/**
 * Format timestamp for X-axis ticks with dynamic precision based on zoom level
 */
export function formatTimeOnlyTick(
	ts: number,
	_idx?: number,
	_ticks?: number[],
	_scale?: uPlot.Scale
) {
	const ms = ts < 100_000_000_000 ? ts * 1000 : ts;
	const d = new Date(ms);

	// If scale provided, check zoom level (currently just showing HH:MM regardless of seconds preference)
	// We keep HH:MM as the most precise format.

	return new Intl.DateTimeFormat('pl-PL', {
		hour: '2-digit',
		minute: '2-digit'
	}).format(d);
}
