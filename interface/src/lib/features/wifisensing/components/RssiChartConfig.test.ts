import { describe, it, expect } from 'vitest';
import {
	calculateVarianceRange,
	calculateYRange,
	createChartOptions,
	getChartColor
} from './RssiChartConfig';
import { CHART_COLORS } from '$lib/constants';

describe('RssiChartConfig', () => {
	it('returns default RSSI range for empty data', () => {
		expect(calculateYRange([])).toEqual([-70, -40]);
	});

	it('enforces minimum RSSI span with centered range', () => {
		// Range is 5 dBm, so it should expand to 15 dBm around center (-57.5)
		expect(calculateYRange([-60, -55])).toEqual([-65, -50]);
	});

	it('pads RSSI range when data span is large enough', () => {
		expect(calculateYRange([-80, -50])).toEqual([-85, -45]);
	});

	it('returns variance range with minimum scale and headroom', () => {
		expect(calculateVarianceRange([1, 2, 3])).toEqual([0, 30]);
		expect(calculateVarianceRange([100])).toEqual([0, 111]);
	});

	it('uses motion/inactive chart colors', () => {
		expect(getChartColor(true)).toBe(CHART_COLORS.motion);
		expect(getChartColor(false)).toBe(CHART_COLORS.inactive);
	});

	it('respects axes visibility and ranges in chart options', () => {
		const optsWithAxes = createChartOptions(
			540,
			120,
			true,
			true,
			[-80, -50],
			[0, 40],
			'#00f',
			false
		);
		expect(optsWithAxes.scales?.y?.range).toEqual([-80, -50]);
		expect(optsWithAxes.scales?.y2?.range).toEqual([0, 40]);
		expect(optsWithAxes.axes?.[1].show).toBe(true);
		expect(optsWithAxes.axes?.[1].size).toBe(55);
		expect(optsWithAxes.axes?.[2].show).toBe(true);
		expect(optsWithAxes.axes?.[2].size).toBe(40);

		const optsRssiOnly = createChartOptions(
			400,
			120,
			true,
			false,
			[-70, -40],
			[0, 30],
			'#00f',
			false
		);
		expect(optsRssiOnly.axes?.[1].show).toBe(true);
		expect(optsRssiOnly.axes?.[1].size).toBe(55);
		expect(optsRssiOnly.axes?.[2].show).toBe(false);
		expect(optsRssiOnly.axes?.[2].size).toBe(0);

		const optsNoAxes = createChartOptions(
			320,
			120,
			false,
			false,
			[-70, -40],
			[0, 30],
			'#00f',
			false
		);
		expect(optsNoAxes.axes?.[1].show).toBe(false);
		expect(optsNoAxes.axes?.[1].size).toBe(0);
		expect(optsNoAxes.axes?.[2].show).toBe(false);
		expect(optsNoAxes.axes?.[2].size).toBe(0);
	});
});
