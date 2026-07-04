import { describe, expect, it } from 'vitest';
import { AIR_MOUSE_CLICK_ACTION, AIR_MOUSE_LOCAL_DEFAULTS } from './airMouseConfig';
import {
	applyCombinedActionValue,
	createAirMouseActionOptions,
	createAirMouseSourceOptions,
	getCombinedActionValue
} from './airMouseSettingsModel';

const LABELS = {
	actionNone: 'None',
	actionLeft: 'Left',
	actionRight: 'Right',
	actionMiddle: 'Middle',
	scriptsSeparator: 'Scripts',
	missingScriptSuffix: 'missing',
	sourceSensor: 'Sensor',
	sourceButton: 'Button'
};

describe('airMouseSettingsModel', () => {
	it('keeps combined action values stable for script and non-script actions', () => {
		expect(getCombinedActionValue(AIR_MOUSE_CLICK_ACTION.LEFT, '')).toBe('1');
		expect(getCombinedActionValue(AIR_MOUSE_CLICK_ACTION.SCRIPT, 'macro_1')).toBe('4|macro_1');
	});

	it('applies combined action values back into the mutable settings object', () => {
		const settings = { ...AIR_MOUSE_LOCAL_DEFAULTS };

		applyCombinedActionValue(settings, '4|macro_2', 'double_click_action', 'double_click_script');
		expect(settings.double_click_action).toBe(AIR_MOUSE_CLICK_ACTION.SCRIPT);
		expect(settings.double_click_script).toBe('macro_2');

		applyCombinedActionValue(settings, '2', 'double_click_action', 'double_click_script');
		expect(settings.double_click_action).toBe(AIR_MOUSE_CLICK_ACTION.RIGHT);
		expect(settings.double_click_script).toBe('');
	});

	it('adds missing selected scripts so selects stay readable after macro deletion', () => {
		const settings = {
			...AIR_MOUSE_LOCAL_DEFAULTS,
			single_click_action: AIR_MOUSE_CLICK_ACTION.SCRIPT,
			single_click_script: 'deleted_macro'
		};

		const options = createAirMouseActionOptions([{ name: 'available_macro' }], settings, LABELS);

		expect(options).toContainEqual({
			value: '4|deleted_macro',
			label: 'deleted_macro (missing)'
		});
	});

	it('creates source options with stable numeric values', () => {
		expect(createAirMouseSourceOptions(LABELS)).toEqual([
			{ value: 0, label: 'Sensor' },
			{ value: 1, label: 'Button' }
		]);
	});
});
