import type { MatrixSettings } from '$lib/services/api/core/MatrixApiService';

export type MatrixSettingsKey = keyof MatrixSettings;
export type MatrixEffectEngine = 0 | 1;
export type MatrixEffectReactivityProvider = 0 | 1;
export type MatrixEffectSpeedScale = 'ms' | 's' | 'm' | 'h';
export type MatrixEffectCategoryId = 'recommended' | 'calm' | 'dynamic' | 'seasonal' | 'all';
export type MatrixBackgroundMode = 0 | 1;
export type MatrixDataVisualizationSource = 0 | 1 | 2 | 3;
export type MatrixDataVisualizationMetric = 0 | 1 | 2 | 3 | 4 | 5;
export type MatrixDataVisualizationMode = 0 | 1 | 2 | 3 | 4 | 5 | 6;
export type MatrixDataVisualizationStaleBehavior = 0 | 1 | 2;

export type MatrixDataVisualizationPreset = Pick<
	MatrixSettings,
	| 'data_visualization_mode'
	| 'data_visualization_min'
	| 'data_visualization_max'
	| 'data_visualization_color_min'
	| 'data_visualization_color_mid'
	| 'data_visualization_color_max'
	| 'data_visualization_brightness_min'
	| 'data_visualization_brightness_max'
	| 'data_visualization_smoothing'
>;

export type MatrixEffectCategoryDefinition = {
	value: MatrixEffectCategoryId;
	effectIds: number[];
};

export type MatrixColorPresetDefinition = {
	id: string;
	colors: [number, number, number];
};

export const DEFAULT_MATRIX_SETTINGS: MatrixSettings = {
	brightness: 20,
	alarm_mode: 1,
	rotation: 0,
	auto_rotate: false,
	effect_enabled: false,
	effect_engine: 0,
	effect_mode: 0,
	effect_speed: 1000,
	effect_color: 0x00ff00,
	effect_color_2: 0xff0000,
	effect_color_3: 0x0000ff,
	effect_reactivity_provider: 0,
	effect_reactivity_gain: 80,
	background_mode: 0,
	data_visualization_enabled: false,
	data_visualization_source: 0,
	data_visualization_metric: 0,
	data_visualization_mode: 0,
	data_visualization_min: 400,
	data_visualization_max: 2000,
	data_visualization_color_min: 0x00ff80,
	data_visualization_color_mid: 0xffd166,
	data_visualization_color_max: 0xff3000,
	data_visualization_brightness_min: 12,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 50,
	data_visualization_stale_behavior: 0,
	data_visualization_device_id: '',
	menu_enabled: true,
	menu_text_color: 0xffffff,
	menu_scroll_speed: 20
};

export const MATRIX_DISPLAY_SETTING_KEYS = [
	'brightness',
	'rotation',
	'auto_rotate',
	'menu_enabled',
	'menu_text_color',
	'menu_scroll_speed'
] as const satisfies readonly MatrixSettingsKey[];

export const MATRIX_ALARM_SETTING_KEYS = [
	'alarm_mode',
	'custom_icons'
] as const satisfies readonly MatrixSettingsKey[];

export const MATRIX_EFFECT_SETTING_KEYS = [
	'background_mode',
	'effect_enabled',
	'data_visualization_enabled',
	'effect_engine',
	'effect_mode',
	'effect_speed',
	'effect_color',
	'effect_color_2',
	'effect_color_3',
	'effect_reactivity_provider',
	'effect_reactivity_gain'
] as const satisfies readonly MatrixSettingsKey[];

export const MATRIX_DATA_VISUALIZATION_SETTING_KEYS = [
	'background_mode',
	'effect_enabled',
	'data_visualization_enabled',
	'data_visualization_source',
	'data_visualization_metric',
	'data_visualization_mode',
	'data_visualization_min',
	'data_visualization_max',
	'data_visualization_color_min',
	'data_visualization_color_mid',
	'data_visualization_color_max',
	'data_visualization_brightness_min',
	'data_visualization_brightness_max',
	'data_visualization_smoothing',
	'data_visualization_stale_behavior',
	'data_visualization_device_id'
] as const satisfies readonly MatrixSettingsKey[];

// Keep this aligned with the backend validator in MatrixConfigJson.cpp.
export const MATRIX_EFFECT_SPEED_MIN = 50;
export const MATRIX_EFFECT_SPEED_MAX = 24 * 60 * 60 * 1000;

export const MATRIX_EFFECT_SPEED_SCALE_CONFIG = {
	ms: { min: 50, max: 5000, step: 50, unitMs: 1, suffix: 'ms' },
	s: { min: 1, max: 60, step: 1, unitMs: 1000, suffix: 's' },
	m: { min: 1, max: 180, step: 1, unitMs: 60 * 1000, suffix: 'm' },
	h: { min: 1, max: 24, step: 1, unitMs: 60 * 60 * 1000, suffix: 'h' }
} as const satisfies Record<
	MatrixEffectSpeedScale,
	{ min: number; max: number; step: number; unitMs: number; suffix: string }
>;

export const MATRIX_EFFECT_SPEED_SCALES: MatrixEffectSpeedScale[] = ['ms', 's', 'm', 'h'];
const MATRIX_COLOR_MAX = 0xffffff;
const MATRIX_CUSTOM_ICON_SLOTS = 3;
export const MATRIX_CUSTOM_ICON_PIXELS = 64;

// Keep this aligned with the backend validator. Matrix effects use one compact
// 0..69 range end-to-end, with no hidden holes or excluded vendor IDs.
export const MATRIX_EFFECT_MODE_MAX = 69;
export const MATRIX_NATIVE_3D_EFFECT_MODE_MAX = 14;
export const MATRIX_EFFECT_ENGINE_LEGACY = 0 satisfies MatrixEffectEngine;
export const MATRIX_EFFECT_ENGINE_NATIVE_3D = 1 satisfies MatrixEffectEngine;
export const MATRIX_REACTIVITY_PROVIDER_NONE = 0 satisfies MatrixEffectReactivityProvider;
export const MATRIX_REACTIVITY_PROVIDER_IMU = 1 satisfies MatrixEffectReactivityProvider;
export const MATRIX_EFFECT_REACTIVITY_GAIN_MIN = 0;
export const MATRIX_EFFECT_REACTIVITY_GAIN_MAX = 200;
export const MATRIX_BACKGROUND_MODE_EFFECTS = 0 satisfies MatrixBackgroundMode;
export const MATRIX_BACKGROUND_MODE_DATA_VISUALIZATION = 1 satisfies MatrixBackgroundMode;
export const MATRIX_DATA_SOURCE_SCD4X = 0 satisfies MatrixDataVisualizationSource;
export const MATRIX_DATA_SOURCE_BLE = 1 satisfies MatrixDataVisualizationSource;
export const MATRIX_DATA_SOURCE_WIFI_RSSI = 2 satisfies MatrixDataVisualizationSource;
export const MATRIX_DATA_SOURCE_WIFI_CSI = 3 satisfies MatrixDataVisualizationSource;
export const MATRIX_DATA_METRIC_CO2 = 0 satisfies MatrixDataVisualizationMetric;
export const MATRIX_DATA_METRIC_TEMPERATURE = 1 satisfies MatrixDataVisualizationMetric;
export const MATRIX_DATA_METRIC_HUMIDITY = 2 satisfies MatrixDataVisualizationMetric;
export const MATRIX_DATA_METRIC_RSSI = 3 satisfies MatrixDataVisualizationMetric;
export const MATRIX_DATA_METRIC_SIGNAL_QUALITY = 4 satisfies MatrixDataVisualizationMetric;
export const MATRIX_DATA_METRIC_CSI_MOTION = 5 satisfies MatrixDataVisualizationMetric;
export const MATRIX_DATA_VIZ_MODE_GAUGE = 0 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_VIZ_MODE_CENTER_RIPPLE = 1 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_VIZ_MODE_HEATMAP = 2 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_VIZ_MODE_TREND = 3 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_VIZ_MODE_SPECTRUM_BARS = 4 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_VIZ_MODE_PERIMETER_METER = 5 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_VIZ_MODE_PULSE = 6 satisfies MatrixDataVisualizationMode;
export const MATRIX_DATA_STALE_DIM = 0 satisfies MatrixDataVisualizationStaleBehavior;
export const MATRIX_DATA_STALE_GRAY = 1 satisfies MatrixDataVisualizationStaleBehavior;
export const MATRIX_DATA_STALE_BLANK = 2 satisfies MatrixDataVisualizationStaleBehavior;

const MATRIX_DATA_PRESET_CO2 = {
	data_visualization_mode: MATRIX_DATA_VIZ_MODE_GAUGE,
	data_visualization_min: 400,
	data_visualization_max: 2000,
	data_visualization_color_min: 0x00ff80,
	data_visualization_color_mid: 0xffd166,
	data_visualization_color_max: 0xff3000,
	data_visualization_brightness_min: 12,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 50
} as const satisfies MatrixDataVisualizationPreset;

const MATRIX_DATA_PRESET_TEMPERATURE = {
	data_visualization_mode: MATRIX_DATA_VIZ_MODE_GAUGE,
	data_visualization_min: 16,
	data_visualization_max: 32,
	data_visualization_color_min: 0x1e88e5,
	data_visualization_color_mid: 0x00ff80,
	data_visualization_color_max: 0xff3000,
	data_visualization_brightness_min: 12,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 50
} as const satisfies MatrixDataVisualizationPreset;

const MATRIX_DATA_PRESET_HUMIDITY = {
	data_visualization_mode: MATRIX_DATA_VIZ_MODE_GAUGE,
	data_visualization_min: 20,
	data_visualization_max: 80,
	data_visualization_color_min: 0xffb000,
	data_visualization_color_mid: 0x00ff80,
	data_visualization_color_max: 0x00b7ff,
	data_visualization_brightness_min: 12,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 50
} as const satisfies MatrixDataVisualizationPreset;

const MATRIX_DATA_PRESET_RSSI_TREND = {
	data_visualization_mode: MATRIX_DATA_VIZ_MODE_TREND,
	data_visualization_min: -90,
	data_visualization_max: -35,
	data_visualization_color_min: 0xff3000,
	data_visualization_color_mid: 0xffd166,
	data_visualization_color_max: 0x00ff80,
	data_visualization_brightness_min: 10,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 60
} as const satisfies MatrixDataVisualizationPreset;

const MATRIX_DATA_PRESET_SIGNAL_QUALITY = {
	data_visualization_mode: MATRIX_DATA_VIZ_MODE_TREND,
	data_visualization_min: 0,
	data_visualization_max: 100,
	data_visualization_color_min: 0xff3000,
	data_visualization_color_mid: 0xffd166,
	data_visualization_color_max: 0x00ff80,
	data_visualization_brightness_min: 10,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 55
} as const satisfies MatrixDataVisualizationPreset;

const MATRIX_DATA_PRESET_CSI = {
	data_visualization_mode: MATRIX_DATA_VIZ_MODE_HEATMAP,
	data_visualization_min: 0,
	data_visualization_max: 100,
	data_visualization_color_min: 0x0040ff,
	data_visualization_color_mid: 0x00ff80,
	data_visualization_color_max: 0xff3000,
	data_visualization_brightness_min: 8,
	data_visualization_brightness_max: 220,
	data_visualization_smoothing: 45
} as const satisfies MatrixDataVisualizationPreset;

export function getDefaultMatrixDataVisualizationMetric(
	source: MatrixDataVisualizationSource
): MatrixDataVisualizationMetric {
	switch (source) {
		case MATRIX_DATA_SOURCE_BLE:
			return MATRIX_DATA_METRIC_TEMPERATURE;
		case MATRIX_DATA_SOURCE_WIFI_RSSI:
			return MATRIX_DATA_METRIC_SIGNAL_QUALITY;
		case MATRIX_DATA_SOURCE_WIFI_CSI:
			return MATRIX_DATA_METRIC_CSI_MOTION;
		case MATRIX_DATA_SOURCE_SCD4X:
		default:
			return MATRIX_DATA_METRIC_CO2;
	}
}

export function getMatrixDataVisualizationPreset(
	source: MatrixDataVisualizationSource,
	metric: MatrixDataVisualizationMetric
): MatrixDataVisualizationPreset {
	switch (source) {
		case MATRIX_DATA_SOURCE_BLE:
			switch (metric) {
				case MATRIX_DATA_METRIC_HUMIDITY:
					return MATRIX_DATA_PRESET_HUMIDITY;
				case MATRIX_DATA_METRIC_RSSI:
					return MATRIX_DATA_PRESET_RSSI_TREND;
				case MATRIX_DATA_METRIC_TEMPERATURE:
				default:
					return MATRIX_DATA_PRESET_TEMPERATURE;
			}
		case MATRIX_DATA_SOURCE_WIFI_RSSI:
			return metric === MATRIX_DATA_METRIC_RSSI
				? MATRIX_DATA_PRESET_RSSI_TREND
				: MATRIX_DATA_PRESET_SIGNAL_QUALITY;
		case MATRIX_DATA_SOURCE_WIFI_CSI:
			return MATRIX_DATA_PRESET_CSI;
		case MATRIX_DATA_SOURCE_SCD4X:
		default:
			switch (metric) {
				case MATRIX_DATA_METRIC_TEMPERATURE:
					return MATRIX_DATA_PRESET_TEMPERATURE;
				case MATRIX_DATA_METRIC_HUMIDITY:
					return MATRIX_DATA_PRESET_HUMIDITY;
				case MATRIX_DATA_METRIC_CO2:
				default:
					return MATRIX_DATA_PRESET_CO2;
			}
	}
}

export const MATRIX_EFFECT_IDS = Array.from(
	{ length: MATRIX_EFFECT_MODE_MAX + 1 },
	(_, effectId) => effectId
);
export const MATRIX_NATIVE_3D_EFFECT_IDS = Array.from(
	{ length: MATRIX_NATIVE_3D_EFFECT_MODE_MAX + 1 },
	(_, effectId) => effectId
);

export const MATRIX_EFFECT_CATEGORIES: MatrixEffectCategoryDefinition[] = [
	{
		value: 'recommended',
		effectIds: [2, 11, 18, 44, 64]
	},
	{
		value: 'calm',
		effectIds: [0, 2, 11, 15, 18, 40, 64]
	},
	{
		value: 'dynamic',
		effectIds: [3, 11, 12, 16, 17, 31, 33, 44, 67, 69]
	},
	{
		value: 'seasonal',
		effectIds: [45, 47, 48, 49, 50, 52, 56, 63]
	},
	{
		value: 'all',
		effectIds: MATRIX_EFFECT_IDS
	}
];

export const MATRIX_NATIVE_3D_EFFECT_CATEGORIES: MatrixEffectCategoryDefinition[] = [
	{
		value: 'recommended',
		effectIds: [0, 1, 3, 4, 5, 7, 10, 14]
	},
	{
		value: 'calm',
		effectIds: [0, 3, 4, 5, 6, 10, 13, 14]
	},
	{
		value: 'dynamic',
		effectIds: [1, 2, 7, 8, 9, 11, 12]
	},
	{
		value: 'seasonal',
		effectIds: [5, 10, 12]
	},
	{
		value: 'all',
		effectIds: MATRIX_NATIVE_3D_EFFECT_IDS
	}
];

export const MATRIX_COLOR_PRESETS: MatrixColorPresetDefinition[] = [
	{
		id: 'alert',
		colors: [0x7f0000, 0xff5a36, 0xffd166]
	},
	{
		id: 'forest',
		colors: [0x0b3d20, 0x2e7d32, 0xa5d6a7]
	},
	{
		id: 'ocean',
		colors: [0x003049, 0x0077b6, 0x90e0ef]
	},
	{
		id: 'sunset',
		colors: [0x5f0f40, 0xfb8b24, 0xffbe0b]
	},
	{
		id: 'neon',
		colors: [0xff006e, 0x8338ec, 0x3a86ff]
	},
	{
		id: 'aurora',
		colors: [0x2ec4b6, 0x7b2cbf, 0xc2f970]
	}
];

export function getMatrixEffectCategory(
	categoryId: MatrixEffectCategoryId,
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): MatrixEffectCategoryDefinition | undefined {
	return getMatrixEffectCategories(engine).find((category) => category.value === categoryId);
}

export function matrixEffectCategoryContainsEffect(
	categoryId: MatrixEffectCategoryId,
	effectId: number,
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): boolean {
	return getMatrixEffectCategory(categoryId, engine)?.effectIds.includes(effectId) ?? false;
}

export function getMatrixEffectCategories(
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): MatrixEffectCategoryDefinition[] {
	return engine === MATRIX_EFFECT_ENGINE_NATIVE_3D
		? MATRIX_NATIVE_3D_EFFECT_CATEGORIES
		: MATRIX_EFFECT_CATEGORIES;
}

export function getMatrixEffectIds(
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): number[] {
	return engine === MATRIX_EFFECT_ENGINE_NATIVE_3D
		? MATRIX_NATIVE_3D_EFFECT_IDS
		: MATRIX_EFFECT_IDS;
}

export function getPreferredMatrixEffectCategory(
	effectId: number,
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): MatrixEffectCategoryId {
	for (const category of getMatrixEffectCategories(engine)) {
		if (category.value === 'all') continue;
		if (category.effectIds.includes(effectId)) {
			return category.value;
		}
	}

	return 'all';
}

export function normalizeMatrixEffectModeForEngine(
	effectId: number,
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): number {
	const max = engine === MATRIX_EFFECT_ENGINE_NATIVE_3D
		? MATRIX_NATIVE_3D_EFFECT_MODE_MAX
		: MATRIX_EFFECT_MODE_MAX;
	if (!Number.isFinite(effectId)) return 0;
	return Math.min(max, Math.max(0, Math.trunc(effectId)));
}

export function normalizeMatrixEffectEngine(value: number): MatrixEffectEngine {
	return value === MATRIX_EFFECT_ENGINE_NATIVE_3D
		? MATRIX_EFFECT_ENGINE_NATIVE_3D
		: MATRIX_EFFECT_ENGINE_LEGACY;
}

function clampMatrixEffectSpeed(value: number): number {
	if (!Number.isFinite(value)) return DEFAULT_MATRIX_SETTINGS.effect_speed;
	return Math.min(MATRIX_EFFECT_SPEED_MAX, Math.max(MATRIX_EFFECT_SPEED_MIN, Math.round(value)));
}

function snapScaleValue(value: number, scale: MatrixEffectSpeedScale): number {
	const { min, max, step } = MATRIX_EFFECT_SPEED_SCALE_CONFIG[scale];
	const clamped = Math.min(max, Math.max(min, value));
	const snapped = min + Math.round((clamped - min) / step) * step;
	return Number(snapped.toFixed(2));
}

export function getPreferredMatrixEffectSpeedScale(effectSpeedMs: number): MatrixEffectSpeedScale {
	const normalized = clampMatrixEffectSpeed(effectSpeedMs);

	if (normalized < 1000) return 'ms';
	if (normalized < 60 * 1000) return 's';
	if (normalized < 60 * 60 * 1000) return 'm';
	return 'h';
}

export function toMatrixEffectSpeedScaleValue(
	effectSpeedMs: number,
	scale: MatrixEffectSpeedScale
): number {
	const normalized = clampMatrixEffectSpeed(effectSpeedMs);
	return snapScaleValue(normalized / MATRIX_EFFECT_SPEED_SCALE_CONFIG[scale].unitMs, scale);
}

export function fromMatrixEffectSpeedScaleValue(
	value: number,
	scale: MatrixEffectSpeedScale
): number {
	const snapped = snapScaleValue(value, scale);
	return clampMatrixEffectSpeed(snapped * MATRIX_EFFECT_SPEED_SCALE_CONFIG[scale].unitMs);
}

export function normalizeMatrixEffectSpeedForScale(
	effectSpeedMs: number,
	scale: MatrixEffectSpeedScale
): number {
	return fromMatrixEffectSpeedScaleValue(
		toMatrixEffectSpeedScaleValue(effectSpeedMs, scale),
		scale
	);
}

export function normalizeMatrixColor(value: number): number {
	const normalized = Number.isFinite(value) ? Math.max(0, Math.trunc(value)) : 0;
	return normalized & MATRIX_COLOR_MAX;
}

export function toMatrixHexColor(value: number): string {
	return `#${normalizeMatrixColor(value).toString(16).padStart(6, '0')}`;
}

export function fromMatrixHexColor(value: string): number {
	const parsed = Number.parseInt(value.replace(/^#/, ''), 16);
	return Number.isFinite(parsed) ? normalizeMatrixColor(parsed) : 0;
}

export function normalizeMatrixCustomIcons(value?: number[][]): number[][] {
	return Array.from({ length: MATRIX_CUSTOM_ICON_SLOTS }, (_, slot) => {
		const pixels = value?.[slot];
		if (!Array.isArray(pixels) || pixels.length !== MATRIX_CUSTOM_ICON_PIXELS) {
			return [];
		}

		return pixels.map(normalizeMatrixColor);
	});
}

export function getMatrixCustomIcons(value?: number[][]): number[][] {
	return normalizeMatrixCustomIcons(value);
}
