import { describe, expect, it } from 'vitest';
import { normalizeCsiAlarmSettings } from './useCsiAlarmConfig.svelte';

describe('normalizeCsiAlarmSettings', () => {
	it('stores selected bands with ascending start and end', () => {
		const settings = normalizeCsiAlarmSettings({
			enabled: true,
			bands: [
				{ start: 70, end: 58 },
				{ start: 10, end: 12 }
			]
		});

		expect(settings.bands).toEqual([
			{ start: 58, end: 70 },
			{ start: 10, end: 12 }
		]);
	});

	it('clamps to the CSI alarm contract', () => {
		const settings = normalizeCsiAlarmSettings({
			bands: [
				{ start: -10, end: 999 },
				{ start: 1, end: 2 },
				{ start: 3, end: 4 },
				{ start: 5, end: 6 },
				{ start: 7, end: 8 }
			],
			baseline_frames: 5,
			top_k: 64,
			enter_threshold: 0,
			clear_threshold: 99,
			hold_ms: 1,
			clear_hold_ms: 99999,
			min_noise: 0,
			min_energy: 99999,
			noisy_threshold: 0,
			sensitivity: 9 as unknown as 0 | 1 | 2
		});

		expect(settings.bands).toHaveLength(4);
		expect(settings.bands[0]).toEqual({ start: 0, end: 255 });
		expect(settings.baseline_frames).toBe(30);
		expect(settings.top_k).toBe(32);
		expect(settings.enter_threshold).toBe(4.5);
		expect(settings.clear_threshold).toBe(2.2);
		expect(settings.hold_ms).toBe(100);
		expect(settings.clear_hold_ms).toBe(30000);
		expect(settings.min_noise).toBe(0.1);
		expect(settings.min_energy).toBe(10000);
		expect(settings.noisy_threshold).toBe(4.5);
		expect(settings.sensitivity).toBe(2);
	});

	it('derives thresholds from sensitivity presets', () => {
		const settings = normalizeCsiAlarmSettings({
			sensitivity: 0,
			enter_threshold: 1,
			clear_threshold: 0.5,
			noisy_threshold: 2
		});

		expect(settings.enter_threshold).toBe(8);
		expect(settings.clear_threshold).toBe(4);
		expect(settings.noisy_threshold).toBe(8);
	});
});
