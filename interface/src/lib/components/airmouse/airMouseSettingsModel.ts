import {
	AIR_MOUSE_CLICK_ACTION,
	AIR_MOUSE_CLICK_SOURCE,
	toAirMouseClickAction,
	type AirMouseLocalSettings,
	type AirMouseClickSource
} from './airMouseConfig';

export type AirMouseActionKey =
	| 'single_click_action'
	| 'double_click_action'
	| 'triple_click_action';

export type AirMouseScriptKey =
	| 'single_click_script'
	| 'double_click_script'
	| 'triple_click_script';

export interface AirMouseSelectOption {
	value: string | number;
	label: string;
	disabled?: boolean;
}

interface AirMouseSettingsLabels {
	actionNone: string;
	actionLeft: string;
	actionRight: string;
	actionMiddle: string;
	scriptsSeparator: string;
	missingScriptSuffix: string;
	sourceSensor: string;
	sourceButton: string;
}

export const AIR_MOUSE_CLICK_BINDINGS: ReadonlyArray<{
	label: 'single' | 'double' | 'triple';
	actionKey: AirMouseActionKey;
	scriptKey: AirMouseScriptKey;
}> = [
	{
		label: 'single',
		actionKey: 'single_click_action',
		scriptKey: 'single_click_script'
	},
	{
		label: 'double',
		actionKey: 'double_click_action',
		scriptKey: 'double_click_script'
	},
	{
		label: 'triple',
		actionKey: 'triple_click_action',
		scriptKey: 'triple_click_script'
	}
] as const;

function createScriptValue(scriptName: string) {
	return `${AIR_MOUSE_CLICK_ACTION.SCRIPT}|${scriptName}`;
}

export function getCombinedActionValue(action: number, script: string) {
	return action === AIR_MOUSE_CLICK_ACTION.SCRIPT ? createScriptValue(script) : action.toString();
}

export function applyCombinedActionValue(
	settings: AirMouseLocalSettings,
	value: string,
	actionKey: AirMouseActionKey,
	scriptKey: AirMouseScriptKey
) {
	const scriptPrefix = `${AIR_MOUSE_CLICK_ACTION.SCRIPT}|`;
	if (value.startsWith(scriptPrefix)) {
		settings[actionKey] = AIR_MOUSE_CLICK_ACTION.SCRIPT;
		settings[scriptKey] = value.substring(scriptPrefix.length);
		return;
	}

	settings[actionKey] = toAirMouseClickAction(Number.parseInt(value, 10));
	settings[scriptKey] = '';
}

export function createAirMouseActionOptions(
	scripts: Array<{ name: string }>,
	settings: AirMouseLocalSettings,
	labels: AirMouseSettingsLabels
): AirMouseSelectOption[] {
	const options: AirMouseSelectOption[] = [
		{ value: AIR_MOUSE_CLICK_ACTION.NONE.toString(), label: labels.actionNone },
		{ value: AIR_MOUSE_CLICK_ACTION.LEFT.toString(), label: labels.actionLeft },
		{ value: AIR_MOUSE_CLICK_ACTION.RIGHT.toString(), label: labels.actionRight },
		{ value: AIR_MOUSE_CLICK_ACTION.MIDDLE.toString(), label: labels.actionMiddle }
	];

	const knownScripts = new Set(scripts.map((script) => script.name));
	const missingSelectedScripts = AIR_MOUSE_CLICK_BINDINGS.flatMap(({ actionKey, scriptKey }) => {
		if (settings[actionKey] !== AIR_MOUSE_CLICK_ACTION.SCRIPT) return [];
		const scriptName = settings[scriptKey];
		if (!scriptName || knownScripts.has(scriptName)) return [];
		return [scriptName];
	});

	if (scripts.length > 0 || missingSelectedScripts.length > 0) {
		options.push({
			value: 'separator',
			label: labels.scriptsSeparator,
			disabled: true
		});
	}

	for (const script of scripts) {
		options.push({
			value: createScriptValue(script.name),
			label: script.name
		});
	}

	for (const scriptName of [...new Set(missingSelectedScripts)]) {
		options.push({
			value: createScriptValue(scriptName),
			label: `${scriptName} (${labels.missingScriptSuffix})`
		});
	}

	return options;
}

export function createAirMouseSourceOptions(
	labels: Pick<AirMouseSettingsLabels, 'sourceSensor' | 'sourceButton'>
): AirMouseSelectOption[] {
	return [
		{ value: AIR_MOUSE_CLICK_SOURCE.SENSOR as AirMouseClickSource, label: labels.sourceSensor },
		{ value: AIR_MOUSE_CLICK_SOURCE.BUTTON as AirMouseClickSource, label: labels.sourceButton }
	];
}
