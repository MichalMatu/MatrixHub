/**
 * Chart series configuration helpers for uPlot
 */
import type uPlotNamespace from 'uplot';

/**
 * Linear path generator — draws straight lines between data points.
 * Preferred for sparse data (≥10min intervals) to avoid spline interpolation
 * artifacts (overshoot, loops) that distort the visual representation.
 */
/**
 * Convert hex color to RGB
 */
function hexToRgb(hex: string): { r: number; g: number; b: number } {
	const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
	return result
		? {
				r: parseInt(result[1], 16),
				g: parseInt(result[2], 16),
				b: parseInt(result[3], 16)
			}
		: { r: 0, g: 0, b: 0 };
}

/**
 * Common series configuration for single line chart.
 *
 * Key design decisions:
 * - Linear paths (no spline) to prevent interpolation artifacts with sparse data
 * - Small data-point dots for visual clarity at 20-min intervals (~72 pts/day)
 * - Subtle fill gradient (0.15 opacity) — visible but doesn't amplify noise
 */
export function createSingleSeriesConfig(
	uPlotLib: typeof uPlotNamespace,
	label: string,
	color: string,
	unit: string,
	decimals: number = 1
) {
	const linearPath = uPlotLib.paths.linear?.();
	if (!linearPath) {
		throw new Error('uPlot linear path helper is unavailable.');
	}

	return [
		{ label: 'Time' },
		{
			label,
			stroke: color,
			width: 2,
			points: {
				show: true,
				size: 4,
				fill: color,
				stroke: color,
				width: 0
			},
			paths: linearPath,
			spanGaps: false,
			fill: (u: uPlotNamespace) => {
				const gradient = u.ctx.createLinearGradient(0, 0, 0, u.bbox.height);
				const rgb = hexToRgb(color);
				gradient.addColorStop(0, `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, 0.15)`);
				gradient.addColorStop(1, `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, 0)`);
				return gradient;
			},
			value: (_u: uPlotNamespace, v: number | null) =>
				v == null ? '-' : v.toFixed(decimals) + ' ' + unit
		}
	];
}
