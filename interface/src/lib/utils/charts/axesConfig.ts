/**
 * Chart axes configuration helpers for uPlot
 */
import type uPlot from 'uplot';
import { formatTimeOnlyTick } from './timeFormatters';

/**
 * Common axis configuration for charts
 */
export const commonAxisStyles = {
	stroke: '#9ca3af',
	grid: { stroke: '#374151', width: 1, dash: [4, 4] as [number, number] },
	ticks: { stroke: '#4b5563', width: 1, size: 4 },
	gap: 3
};

export interface AxesConfigOptions {
	compact?: boolean;
	tiny?: boolean;
}

/**
 * Build a dynamic Y-axis size function.
 * Measures the widest tick label on each redraw and adds padding for the
 * tick marks + gap, so the axis is never wider than it needs to be.
 */
function autoYAxisSize(gap: number, tickSize: number) {
	return (self: uPlot, values: string[], _axisIdx: number): number => {
		if (!values || values.length === 0) return 20;

		// Sync font size with charts.css media queries
		const width = window.innerWidth;
		const fontSize = width <= 320 ? 9 : width <= 420 ? 10 : 12;
		const font = `${fontSize}px "Inter", system-ui, sans-serif`;

		const ctx = self.ctx;
		ctx.save();
		ctx.font = font;

		let maxWidth = 0;
		for (const v of values) {
			const w = ctx.measureText(v).width;
			if (w > maxWidth) maxWidth = w;
		}
		ctx.restore();

		return Math.ceil(maxWidth) + tickSize + gap + 4;
	};
}

/**
 * Common axes configuration for single scale chart
 */
export function createSingleAxesConfig(
	unit: string,
	decimals: number = 0,
	_range?: [number, number] | ((u: uPlot, min: number, max: number) => [number, number]),
	options?: AxesConfigOptions
) {
	const compact = options?.compact === true;
	const tiny = options?.tiny === true;
	const currentGap = tiny ? 2 : compact ? 3 : commonAxisStyles.gap;
	const currentTickSize = tiny ? 2 : compact ? 3 : commonAxisStyles.ticks.size;

	const axisBase = {
		...commonAxisStyles,
		gap: currentGap,
		ticks: {
			...commonAxisStyles.ticks,
			size: currentTickSize
		}
	};

	return [
		{
			...axisBase,
			size: tiny ? 35 : compact ? 38 : 42, // X axis (bottom) — fixed is fine
			values: (u: uPlot, vals: number[]) =>
				vals.map((v, i, arr) => formatTimeOnlyTick(v, i, arr, u.scales.x))
		},
		{
			...axisBase,
			size: autoYAxisSize(currentGap, currentTickSize), // Y axis — dynamic!
			values: (_u: uPlot, vals: number[]) => vals.map((v) => v.toFixed(decimals))
		}
	];
}
