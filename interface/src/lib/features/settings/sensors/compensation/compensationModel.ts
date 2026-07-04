import type { CompensationSettings } from '$lib/services/api/integrations/CompensationApiService';

export const DEFAULT_COMPENSATION_SETTINGS: CompensationSettings = {
	enabled: false,
	base_temp_offset: 2.0,
	reference_cpu_temp: 45.0,
	temp_offset_per_cpu_degree: 0.2,
	min_temp_offset: 0.0,
	max_temp_offset: 8.0
};

const COMPENSATION_LIMITS = {
	base_temp_offset: { min: -5, max: 20 },
	reference_cpu_temp: { min: 20, max: 80 },
	temp_offset_per_cpu_degree: { min: 0, max: 2 },
	min_temp_offset: { min: -10, max: 25 },
	max_temp_offset: { min: -10, max: 25 }
} as const;

export const COMPENSATION_PRESETS = {
	none: {
		base_temp_offset: 0.0,
		temp_offset_per_cpu_degree: 0.0,
		reference_cpu_temp: 40.0,
		min_temp_offset: 0.0,
		max_temp_offset: 15.0
	},
	factory: {
		base_temp_offset: 2.0,
		temp_offset_per_cpu_degree: 0.2,
		reference_cpu_temp: 45.0,
		min_temp_offset: 0.0,
		max_temp_offset: 8.0
	},
	small: {
		base_temp_offset: 2.0,
		temp_offset_per_cpu_degree: 0.1,
		reference_cpu_temp: 40.0,
		min_temp_offset: 0.0,
		max_temp_offset: 10.0
	},
	large: {
		base_temp_offset: 4.0,
		temp_offset_per_cpu_degree: 0.3,
		reference_cpu_temp: 45.0,
		min_temp_offset: 0.0,
		max_temp_offset: 10.0
	}
} as const;

export type CompensationPresetName = keyof typeof COMPENSATION_PRESETS;

// Magnus formula constants (Sensirion / WMO standard)
const MAGNUS_A = 17.62;
const MAGNUS_B = 243.12;

function clamp(value: number, min: number, max: number): number {
	return Math.min(Math.max(value, min), max);
}

function normalizeNumber(value: number, fallback: number, min: number, max: number): number {
	return Number.isFinite(value) ? clamp(value, min, max) : fallback;
}

export function normalizeCompensationSettings(
	settings: Partial<CompensationSettings> | CompensationSettings
): CompensationSettings {
	const minTempOffset = normalizeNumber(
		settings.min_temp_offset ?? DEFAULT_COMPENSATION_SETTINGS.min_temp_offset,
		DEFAULT_COMPENSATION_SETTINGS.min_temp_offset,
		COMPENSATION_LIMITS.min_temp_offset.min,
		COMPENSATION_LIMITS.min_temp_offset.max
	);
	const maxTempOffset = normalizeNumber(
		settings.max_temp_offset ?? DEFAULT_COMPENSATION_SETTINGS.max_temp_offset,
		DEFAULT_COMPENSATION_SETTINGS.max_temp_offset,
		Math.max(minTempOffset, COMPENSATION_LIMITS.max_temp_offset.min),
		COMPENSATION_LIMITS.max_temp_offset.max
	);

	return {
		enabled: Boolean(settings.enabled),
		base_temp_offset: normalizeNumber(
			settings.base_temp_offset ?? DEFAULT_COMPENSATION_SETTINGS.base_temp_offset,
			DEFAULT_COMPENSATION_SETTINGS.base_temp_offset,
			COMPENSATION_LIMITS.base_temp_offset.min,
			COMPENSATION_LIMITS.base_temp_offset.max
		),
		reference_cpu_temp: normalizeNumber(
			settings.reference_cpu_temp ?? DEFAULT_COMPENSATION_SETTINGS.reference_cpu_temp,
			DEFAULT_COMPENSATION_SETTINGS.reference_cpu_temp,
			COMPENSATION_LIMITS.reference_cpu_temp.min,
			COMPENSATION_LIMITS.reference_cpu_temp.max
		),
		temp_offset_per_cpu_degree: normalizeNumber(
			settings.temp_offset_per_cpu_degree ??
				DEFAULT_COMPENSATION_SETTINGS.temp_offset_per_cpu_degree,
			DEFAULT_COMPENSATION_SETTINGS.temp_offset_per_cpu_degree,
			COMPENSATION_LIMITS.temp_offset_per_cpu_degree.min,
			COMPENSATION_LIMITS.temp_offset_per_cpu_degree.max
		),
		min_temp_offset: minTempOffset,
		max_temp_offset: maxTempOffset
	};
}

export function validateCompensationSettings(settings: CompensationSettings) {
	const errors = {
		base_temp_offset:
			settings.base_temp_offset < COMPENSATION_LIMITS.base_temp_offset.min ||
			settings.base_temp_offset > COMPENSATION_LIMITS.base_temp_offset.max,
		reference_cpu_temp:
			settings.reference_cpu_temp < COMPENSATION_LIMITS.reference_cpu_temp.min ||
			settings.reference_cpu_temp > COMPENSATION_LIMITS.reference_cpu_temp.max,
		temp_offset_per_cpu_degree:
			settings.temp_offset_per_cpu_degree < COMPENSATION_LIMITS.temp_offset_per_cpu_degree.min ||
			settings.temp_offset_per_cpu_degree > COMPENSATION_LIMITS.temp_offset_per_cpu_degree.max,
		min_temp_offset:
			settings.min_temp_offset < COMPENSATION_LIMITS.min_temp_offset.min ||
			settings.min_temp_offset > settings.max_temp_offset,
		max_temp_offset:
			settings.max_temp_offset < settings.min_temp_offset ||
			settings.max_temp_offset > COMPENSATION_LIMITS.max_temp_offset.max
	};

	return {
		errors,
		hasError: Object.values(errors).some(Boolean)
	};
}

export function compensateHumidity(rawRH: number, rawTemp: number, compTemp: number): number {
	const gammaRaw = (MAGNUS_A * rawTemp) / (MAGNUS_B + rawTemp);
	const gammaComp = (MAGNUS_A * compTemp) / (MAGNUS_B + compTemp);
	const corrected = rawRH * Math.exp(gammaRaw - gammaComp);
	return clamp(corrected, 0, 100);
}

export function applyCompensationPreset(
	settings: CompensationSettings,
	presetName: CompensationPresetName
): CompensationSettings {
	return {
		...settings,
		...COMPENSATION_PRESETS[presetName]
	};
}

export function calculateCompensationPreview(settings: CompensationSettings, currentCpu: number) {
	const delta = currentCpu - settings.reference_cpu_temp;
	const calculatedRaw = settings.base_temp_offset + delta * settings.temp_offset_per_cpu_degree;
	const correction = clamp(calculatedRaw, settings.min_temp_offset, settings.max_temp_offset);
	const humidityDelta =
		Math.abs(correction) < 0.1
			? null
			: compensateHumidity(50, currentCpu, currentCpu - correction) - 50;

	return {
		currentCpu,
		delta,
		calculatedRaw,
		correction,
		isClamped: calculatedRaw !== correction,
		humidityDelta
	};
}
