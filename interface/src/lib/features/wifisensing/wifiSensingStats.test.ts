import { describe, expect, it } from 'vitest';
import {
	appendWifiSensingSample,
	calculateWifiSensingStats,
	createInitialWifiSensingStats
} from './wifiSensingStats';

describe('wifiSensingStats', () => {
	it('calculates current, min, max, avg and window from samples', () => {
		const stats = calculateWifiSensingStats([
			{ rssi: -60, timestamp: 1000, variance: 1.5 },
			{ rssi: -55, timestamp: 1500, variance: 2.25 },
			{ rssi: -50, timestamp: 2200, variance: 3.75 }
		]);

		expect(stats).toEqual({
			current: -50,
			min: -60,
			max: -50,
			avg: -55,
			variance: 3.75,
			sampleCount: 3,
			windowMs: 1200
		});
	});

	it('keeps only the newest samples within the buffer limit', () => {
		const limited = appendWifiSensingSample(
			[
				{ rssi: -60, timestamp: 1000 },
				{ rssi: -59, timestamp: 1100 }
			],
			{ rssi: -58, timestamp: 1200 },
			2
		);

		expect(limited).toEqual([
			{ rssi: -59, timestamp: 1100 },
			{ rssi: -58, timestamp: 1200 }
		]);
	});

	it('creates empty stats fallback for connection-only state', () => {
		expect(createInitialWifiSensingStats(-47)).toEqual({
			current: -47,
			min: -47,
			max: -47,
			avg: -47,
			variance: 0,
			sampleCount: 0,
			windowMs: 0
		});
	});
});
