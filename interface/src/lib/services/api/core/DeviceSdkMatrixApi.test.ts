import { describe, expect, it } from 'vitest';

import { parseMatrixSettings } from '@matrixhub/device-sdk';

function createMatrixPayload(overrides: Record<string, unknown> = {}) {
	return {
		brightness: 10,
		alarm_mode: 2,
		rotation: 0,
		auto_rotate: false,
		effect_enabled: false,
		effect_engine: 0,
		effect_mode: 0,
		effect_speed: 1000,
		effect_color: 0x1abcdef,
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
		custom_icons: [[], [], []],
		menu_enabled: true,
		menu_text_color: 0x1ffffff,
		menu_scroll_speed: 60,
		...overrides
	};
}

describe('device SDK matrix parser', () => {
	it('normalizes matrix colors to RGB888', () => {
		const parsed = parseMatrixSettings(createMatrixPayload());

		expect(parsed?.effect_color).toBe(0xabcdef);
		expect(parsed?.menu_text_color).toBe(0xffffff);
	});

	it('keeps effect mode and speed inside the firmware-supported range', () => {
		const parsed = parseMatrixSettings(createMatrixPayload({ effect_mode: 255, effect_speed: 1 }));

		expect(parsed?.effect_mode).toBe(69);
		expect(parsed?.effect_speed).toBe(50);
	});

	it('keeps effect engine and reactivity settings inside the firmware-supported range', () => {
		const parsed = parseMatrixSettings(
			createMatrixPayload({
				effect_engine: 9,
				effect_reactivity_provider: 8,
				effect_reactivity_gain: 255
			})
		);

		expect(parsed?.effect_engine).toBe(1);
		expect(parsed?.effect_reactivity_provider).toBe(1);
		expect(parsed?.effect_reactivity_gain).toBe(200);
	});

	it('uses the compact native 3D effect mode range when the native engine is active', () => {
		const parsed = parseMatrixSettings(createMatrixPayload({ effect_engine: 1, effect_mode: 69 }));

		expect(parsed?.effect_engine).toBe(1);
		expect(parsed?.effect_mode).toBe(3);
	});

	it('normalizes matrix data visualization settings', () => {
		const parsed = parseMatrixSettings(
			createMatrixPayload({
				background_mode: 9,
				data_visualization_enabled: true,
				data_visualization_source: 9,
				data_visualization_metric: 9,
				data_visualization_mode: 9,
				data_visualization_min: 30,
				data_visualization_max: 20,
				data_visualization_color_min: 0x1abcdef,
				data_visualization_color_mid: 0x1000000,
				data_visualization_color_max: 0xffffffff,
				data_visualization_brightness_min: 200,
				data_visualization_brightness_max: 10,
				data_visualization_smoothing: 255,
				data_visualization_stale_behavior: 9,
				data_visualization_device_id: 'AA:BB:CC:DD:EE:FF-extra'
			})
		);

		expect(parsed?.background_mode).toBe(1);
		expect(parsed?.data_visualization_enabled).toBe(true);
		expect(parsed?.data_visualization_source).toBe(3);
		expect(parsed?.data_visualization_metric).toBe(5);
		expect(parsed?.data_visualization_mode).toBe(6);
		expect(parsed?.data_visualization_max).toBe(31);
		expect(parsed?.data_visualization_color_min).toBe(0xabcdef);
		expect(parsed?.data_visualization_color_mid).toBe(0x000000);
		expect(parsed?.data_visualization_color_max).toBe(0xffffff);
		expect(parsed?.data_visualization_brightness_min).toBe(10);
		expect(parsed?.data_visualization_brightness_max).toBe(200);
		expect(parsed?.data_visualization_smoothing).toBe(100);
		expect(parsed?.data_visualization_stale_behavior).toBe(2);
		expect(parsed?.data_visualization_device_id).toBe('AA:BB:CC:DD:EE:FF');
	});

	it('accepts exactly three custom icon slots with empty or 64-pixel payloads', () => {
		const icon = Array.from({ length: 64 }, (_, index) => (index === 0 ? 0x1abcdef : index));
		const parsed = parseMatrixSettings(createMatrixPayload({ custom_icons: [icon, [], icon] }));

		expect(parsed?.custom_icons?.[0]?.[0]).toBe(0xabcdef);
		expect(parsed?.custom_icons?.[1]).toEqual([]);
		expect(parsed?.custom_icons?.[2]?.length).toBe(64);
	});

	it('rejects malformed custom icon payloads', () => {
		expect(parseMatrixSettings(createMatrixPayload({ custom_icons: [[1], [], []] }))).toBeNull();
		expect(parseMatrixSettings(createMatrixPayload({ custom_icons: [[], []] }))).toBeNull();
		expect(
			parseMatrixSettings(createMatrixPayload({ custom_icons: [[Number.NaN], [], []] }))
		).toBeNull();
	});
});
