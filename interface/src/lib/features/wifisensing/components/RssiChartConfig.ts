import type uPlotNamespace from 'uplot';
import { CHART_COLORS } from '$lib/constants';
import { commonCursorConfig, commonAxisStyles } from '$lib/utils/charts';
import { formatTimeOnlyTick } from '$lib/utils/charts/timeFormatters';

export function getChartColor(motion: boolean) {
	return motion ? CHART_COLORS.motion : CHART_COLORS.inactive;
}

export function calculateYRange(rssiValues: number[]): [number, number] {
	if (rssiValues.length === 0) return [-70, -40]; // Default range

	let min = rssiValues[0];
	let max = rssiValues[0];
	for (let i = 1; i < rssiValues.length; i++) {
		if (rssiValues[i] < min) min = rssiValues[i];
		if (rssiValues[i] > max) max = rssiValues[i];
	}

	// Add padding of 5 dBm on each side, but ensure minimum range of 15 dBm
	const padding = 5;
	const minRange = 15;
	const range = max - min;

	if (range < minRange) {
		const center = (max + min) / 2;
		return [Math.floor(center - minRange / 2), Math.ceil(center + minRange / 2)];
	}

	return [Math.floor(min - padding), Math.ceil(max + padding)];
}

export function calculateVarianceRange(varianceValues: number[]): [number, number] {
	let max = 0;
	for (let v of varianceValues) {
		if (v > max) max = v;
	}

	// Ensure minimum scale of 0-30 so small noise doesn't look huge
	// Add 10% headroom for high values
	const top = Math.max(30, Math.ceil(max * 1.1));
	return [0, top];
}

export function createChartOptions(
	width: number,
	height: number,
	showRssiAxis: boolean,
	showVarianceAxis: boolean,
	rssiYRange: [number, number],
	varianceRange: [number, number],
	color: string,
	motionDetected: boolean
): uPlotNamespace.Options {
	return {
		width,
		height,
		// Keep the plot compact on narrow screens and only reserve space for the
		// right-side variance axis when it is actually visible.
		padding: [12, showVarianceAxis ? 10 : 0, 0, 0],
		cursor: commonCursorConfig,
		scales: {
			x: { time: false },
			y: {
				auto: false,
				range: rssiYRange
			},
			y2: {
				auto: false,
				range: varianceRange
			}
		},
		series: [
			{ label: 'Time' },
			{
				label: 'RSSI',
				stroke: color,
				width: 2,
				points: { show: false },
				fill: (u: uPlotNamespace) => {
					const gradient = u.ctx.createLinearGradient(0, 0, 0, u.bbox.height);
					if (motionDetected) {
						gradient.addColorStop(0, 'rgba(245, 158, 11, 0.3)');
						gradient.addColorStop(1, 'rgba(245, 158, 11, 0)');
					} else {
						gradient.addColorStop(0, 'rgba(59, 130, 246, 0.3)');
						gradient.addColorStop(1, 'rgba(59, 130, 246, 0)');
					}
					return gradient;
				},
				value: (_u: uPlotNamespace, v: number | null) => (v == null ? '-' : v + ' dBm')
			},
			{
				// Variance Series (Purple)
				label: 'VAR',
				scale: 'y2',
				stroke: '#a855f7', // Purple-500
				width: 1,
				dash: [4, 4],
				points: { show: false },
				value: (_u: uPlotNamespace, v: number | null) => (v == null ? '-' : v.toFixed(2))
			}
		],
		axes: [
			{
				...commonAxisStyles,
				size: 35,
				values: (u, vals) =>
					vals.map((v, index, allValues) => formatTimeOnlyTick(v, index, allValues, u.scales.x))
			},
			{
				...commonAxisStyles,
				show: showRssiAxis,
				size: showRssiAxis ? 55 : 0,
				values: (_u, vals) => vals.map((v) => v + ' dBm')
			},
			{
				...commonAxisStyles,
				scale: 'y2',
				side: 1, // Right side
				show: showVarianceAxis,
				size: showVarianceAxis ? 40 : 0,
				grid: { show: false }, // Avoid grid clutter
				values: (_u, vals) => vals.map((v) => v.toFixed(1))
			}
		],
		legend: { show: false }
	};
}
