import { describe, expect, it } from 'vitest';
import {
	appendDashboardHistoryPoint,
	buildDashboardHistoryState,
	DASHBOARD_HISTORY_POINTS,
	EMPTY_DASHBOARD_HISTORY_STATE
} from './dashboardTelemetry';

describe('dashboardTelemetry', () => {
	it('keeps timestamps aligned when null values are filtered out of a metric history', () => {
		const history = buildDashboardHistoryState({
			timestamps: [100, 200, 300, 400],
			co2: [500, 510, 520, 530],
			temp: [21.1, null, 21.3, 21.4],
			humid: [45.1, 45.2, 45.3, 45.4]
		});

		expect(history.temp.values).toEqual([21.1, 21.3, 21.4]);
		expect(history.temp.timestamps).toEqual([100, 300, 400]);
		expect(history.co2.timestamps).toEqual([100, 200, 300, 400]);
	});

	it('trims dashboard history to the configured sparkline window', () => {
		const timestamps = Array.from(
			{ length: DASHBOARD_HISTORY_POINTS + 4 },
			(_, index) => index + 1
		);
		const co2 = timestamps.map((value) => 400 + value);
		const temp = timestamps.map((value) => 20 + value / 10);
		const humid = timestamps.map((value) => 50 + value / 10);

		const history = buildDashboardHistoryState({ timestamps, co2, temp, humid });

		expect(history.co2.values).toHaveLength(DASHBOARD_HISTORY_POINTS);
		expect(history.co2.timestamps).toHaveLength(DASHBOARD_HISTORY_POINTS);
		expect(history.co2.timestamps[0]).toBe(5);
		expect(history.co2.timestamps.at(-1)).toBe(DASHBOARD_HISTORY_POINTS + 4);
	});

	it('appends live points with per-series timestamps and keeps the tail bounded', () => {
		let history = EMPTY_DASHBOARD_HISTORY_STATE;

		for (let index = 0; index < DASHBOARD_HISTORY_POINTS + 2; index += 1) {
			history = appendDashboardHistoryPoint(
				history,
				{ co2: 500 + index, temp: 20 + index / 10, humid: 40 + index / 10 },
				1000 + index
			);
		}

		expect(history.co2.values).toHaveLength(DASHBOARD_HISTORY_POINTS);
		expect(history.temp.timestamps).toHaveLength(DASHBOARD_HISTORY_POINTS);
		expect(history.co2.values[0]).toBe(502);
		expect(history.humid.timestamps[0]).toBe(1002);
	});
});
