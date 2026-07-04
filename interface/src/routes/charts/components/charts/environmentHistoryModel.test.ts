import { describe, expect, it } from 'vitest';
import {
	buildEnvironmentPlotData,
	buildEnvironmentSeriesModels,
	calculateEnvironmentSeriesStats,
	formatSensorValue,
	getEnvironmentSampleValues,
	getEnvironmentPlotRange,
	projectEnvironmentBand
} from './environmentHistoryModel';

describe('environmentHistoryModel', () => {
	const defaultBand = { min: 10, max: 30 };

	it('keeps a flat series centered inside its sensor band', () => {
		const projected = projectEnvironmentBand([45.5, 45.5, 45.5], 10, defaultBand);

		expect(projected).toEqual([20, 20, 20]);
	});

	it('does not amplify tiny changes to the full sensor band', () => {
		const projected = projectEnvironmentBand([45.5, 45.6], 10, defaultBand);

		expect(projected[0]).toBeCloseTo(19.91, 2);
		expect(projected[1]).toBeCloseTo(20.09, 2);
	});

	it('preserves missing samples while projecting valid values', () => {
		const projected = projectEnvironmentBand([600, null, 700], 200, defaultBand);

		expect(projected[0]).toBeLessThan(20);
		expect(projected[1]).toBeNull();
		expect(projected[2]).toBeGreaterThan(20);
	});

	it('calculates stats from valid samples and uses the last valid value as latest', () => {
		const stats = calculateEnvironmentSeriesStats([null, 10, 12, null, 15]);

		expect(stats).toMatchObject({
			min: 10,
			max: 15,
			avg: 12.333333333333334,
			first: 10,
			latest: 15,
			latestIndex: 4,
			delta: 5,
			range: 5
		});
	});

	it('builds display models and sample values for all environment series', () => {
		const models = buildEnvironmentSeriesModels([
			{
				key: 'co2',
				label: 'CO2',
				color: '#22c55e',
				unit: 'ppm',
				decimals: 0,
				data: [600, 620],
				minPlotRange: 200,
				band: { min: 70, max: 94 }
			},
			{
				key: 'temperature',
				label: 'Temperature',
				color: '#ef4444',
				unit: '°C',
				decimals: 1,
				data: [22.1, 22.6],
				minPlotRange: 5,
				band: { min: 38, max: 62 }
			}
		]);

		const samples = getEnvironmentSampleValues(models, 1);

		expect(models).toHaveLength(2);
		expect(samples).toEqual([
			expect.objectContaining({ key: 'co2', value: 620 }),
			expect.objectContaining({ key: 'temperature', value: 22.6 })
		]);
	});

	it('adds null points to break lines across large time gaps', () => {
		const models = buildEnvironmentSeriesModels([
			{
				key: 'co2',
				label: 'CO2',
				color: '#22c55e',
				unit: 'ppm',
				decimals: 0,
				data: [600, 620],
				minPlotRange: 200,
				band: { min: 70, max: 94 }
			}
		]);

		const plotData = buildEnvironmentPlotData([100, 4000], models, 3600);

		expect(plotData.timestamps).toEqual([100, 2050, 4000]);
		expect(plotData.rawIndexes).toEqual([0, null, 1]);
		expect(plotData.series.co2[1]).toBeNull();
		expect(plotData.series.temperature[1]).toBeNull();
		expect(plotData.series.humidity[1]).toBeNull();
	});

	it('returns padded plot ranges for per-sensor tracks', () => {
		const stats = calculateEnvironmentSeriesStats([10, 20]);
		const range = getEnvironmentPlotRange(stats, 20);

		expect(range?.[0]).toBeCloseTo(3.4, 1);
		expect(range?.[1]).toBeCloseTo(26.6, 1);
	});

	it('formats missing and present sensor values consistently', () => {
		expect(formatSensorValue(612, 0, 'ppm')).toBe('612ppm');
		expect(formatSensorValue(22.456, 1, '°C')).toBe('22.5°C');
		expect(formatSensorValue(null, 1, '%')).toBe('-');
	});
});
