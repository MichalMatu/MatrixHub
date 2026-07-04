import { describe, expect, it } from 'vitest';
import { createAirMouseGyroPolylinePoints, toAirMousePercent } from './airMouseStatusModel';

describe('airMouseStatusModel', () => {
	it('clamps tap force percent to 0..100 and ignores invalid values', () => {
		expect(toAirMousePercent(-1)).toBe(0);
		expect(toAirMousePercent(Number.NaN)).toBe(0);
		expect(toAirMousePercent(2)).toBe(50);
		expect(toAirMousePercent(10)).toBe(100);
		expect(toAirMousePercent(1, 0)).toBe(0);
	});

	it('builds safe gyro polyline points for finite and non-finite history values', () => {
		expect(createAirMouseGyroPolylinePoints([], 32)).toBe('');
		expect(createAirMouseGyroPolylinePoints([Number.NaN, 1000, -1000], 3)).toBe(
			'0.0,0.0 50.0,-48.0 100.0,48.0'
		);
	});

	it('clamps negative offsets when history is longer than the configured window', () => {
		expect(createAirMouseGyroPolylinePoints([1, 2, 3, 4], 2)).toBe('0.0,-1.5 100.0,-2.0');
	});
});
