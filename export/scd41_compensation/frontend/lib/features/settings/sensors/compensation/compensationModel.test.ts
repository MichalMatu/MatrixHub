import { describe, expect, it } from 'vitest';
import {
	COMPENSATION_PRESETS,
	DEFAULT_COMPENSATION_SETTINGS,
	applyCompensationPreset,
	calculateCompensationPreview,
	compensateHumidity,
	normalizeCompensationSettings,
	validateCompensationSettings
} from './compensationModel';

describe('compensationModel', () => {
	it('normalizes invalid API values into safe bounds', () => {
		expect(
			normalizeCompensationSettings({
				enabled: 1 as never,
				base_temp_offset: 999,
				reference_cpu_temp: 5,
				temp_offset_per_cpu_degree: -1,
				min_temp_offset: 30,
				max_temp_offset: -4
			})
		).toEqual({
			enabled: true,
			base_temp_offset: 20,
			reference_cpu_temp: 20,
			temp_offset_per_cpu_degree: 0,
			min_temp_offset: 25,
			max_temp_offset: 25
		});
	});

	it('calculates preview and clamped humidity delta', () => {
		const preview = calculateCompensationPreview(
			{
				...DEFAULT_COMPENSATION_SETTINGS,
				base_temp_offset: 4,
				reference_cpu_temp: 40,
				temp_offset_per_cpu_degree: 0.5,
				min_temp_offset: 0,
				max_temp_offset: 6
			},
			50
		);

		expect(preview.calculatedRaw).toBe(9);
		expect(preview.correction).toBe(6);
		expect(preview.isClamped).toBe(true);
		expect(preview.humidityDelta).not.toBeNull();
		expect(preview.humidityDelta).toBeGreaterThan(0);
	});

	it('applies presets without mutating base settings and validates inverted bounds', () => {
		const updated = applyCompensationPreset(DEFAULT_COMPENSATION_SETTINGS, 'large');
		expect(updated).toEqual({
			...DEFAULT_COMPENSATION_SETTINGS,
			...COMPENSATION_PRESETS.large
		});
		expect(DEFAULT_COMPENSATION_SETTINGS).toEqual({
			enabled: false,
			base_temp_offset: 2,
			reference_cpu_temp: 45,
			temp_offset_per_cpu_degree: 0.2,
			min_temp_offset: 0,
			max_temp_offset: 8
		});

		const validation = validateCompensationSettings({
			...updated,
			min_temp_offset: 5,
			max_temp_offset: 2
		});
		expect(validation.hasError).toBe(true);
		expect(validation.errors.min_temp_offset).toBe(true);
		expect(validation.errors.max_temp_offset).toBe(true);
	});

	it('clamps compensated humidity to 100 percent', () => {
		expect(compensateHumidity(98, 60, 20)).toBe(100);
	});
});
