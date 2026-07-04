export const AIR_MOUSE_CLICK_SOURCE = {
	SENSOR: 0,
	BUTTON: 1
} as const;

export const AIR_MOUSE_CLICK_ACTION = {
	NONE: 0,
	LEFT: 1,
	RIGHT: 2,
	MIDDLE: 3,
	SCRIPT: 4
} as const;

export type AirMouseClickSource =
	(typeof AIR_MOUSE_CLICK_SOURCE)[keyof typeof AIR_MOUSE_CLICK_SOURCE];
export type AirMouseClickAction =
	(typeof AIR_MOUSE_CLICK_ACTION)[keyof typeof AIR_MOUSE_CLICK_ACTION];

export interface AirMouseSettingsDefaults {
	movement_enabled: boolean;
	click_enabled: boolean;
	sensitivityX: number;
	sensitivityY: number;
	deadzone: number;
	accelerationFactor: number;
	tapThreshold: number;
	clickDebounce: number;
	doubleClickWindow: number;
	click_source: AirMouseClickSource;
	single_click_action: AirMouseClickAction;
	double_click_action: AirMouseClickAction;
	triple_click_action: AirMouseClickAction;
	single_click_script: string;
	double_click_script: string;
	triple_click_script: string;
}

export type AirMouseLocalSettings = AirMouseSettingsDefaults;

const CLICK_SOURCE_VALUES = Object.values(AIR_MOUSE_CLICK_SOURCE) as AirMouseClickSource[];
const CLICK_ACTION_VALUES = Object.values(AIR_MOUSE_CLICK_ACTION) as AirMouseClickAction[];

export const AIR_MOUSE_JIGGLER_MODES = {
	OFF: 0,
	STEALTH: 1,
	ACTIVE: 2,
	HUMAN: 3,
	KEYBOARD: 4
} as const;

export const toAirMouseClickSource = (value: number | null | undefined): AirMouseClickSource => {
	if (value === null || value === undefined) return AIR_MOUSE_CLICK_SOURCE.SENSOR;
	return CLICK_SOURCE_VALUES.includes(value as AirMouseClickSource)
		? (value as AirMouseClickSource)
		: AIR_MOUSE_CLICK_SOURCE.SENSOR;
};

export const toAirMouseClickAction = (value: number | null | undefined): AirMouseClickAction => {
	if (value === null || value === undefined) return AIR_MOUSE_CLICK_ACTION.NONE;
	return CLICK_ACTION_VALUES.includes(value as AirMouseClickAction)
		? (value as AirMouseClickAction)
		: AIR_MOUSE_CLICK_ACTION.NONE;
};

const BASE_SETTINGS_DEFAULTS: AirMouseSettingsDefaults = {
	movement_enabled: true,
	click_enabled: true,
	sensitivityX: 200,
	sensitivityY: 200,
	deadzone: 2.0,
	accelerationFactor: 1.0,
	tapThreshold: 0.4,
	clickDebounce: 200,
	doubleClickWindow: 500,
	click_source: AIR_MOUSE_CLICK_SOURCE.SENSOR,
	single_click_action: AIR_MOUSE_CLICK_ACTION.LEFT,
	double_click_action: AIR_MOUSE_CLICK_ACTION.RIGHT,
	triple_click_action: AIR_MOUSE_CLICK_ACTION.NONE,
	single_click_script: '',
	double_click_script: '',
	triple_click_script: ''
};

export const AIR_MOUSE_DEFAULTS: AirMouseSettingsDefaults = BASE_SETTINGS_DEFAULTS;

export const AIR_MOUSE_LOCAL_DEFAULTS: AirMouseLocalSettings = {
	...BASE_SETTINGS_DEFAULTS
};

export const AIR_MOUSE_JIGGLER_DEFAULTS = {
	mode: AIR_MOUSE_JIGGLER_MODES.OFF,
	interval: 60,
	distance: 1,
	random: false
} as const;

export const AIR_MOUSE_JIGGLER_LIMITS = {
	interval: { min: 1, max: 3600, step: 1 },
	distance: { min: 1, max: 5000, step: 10 }
} as const;

export const AIR_MOUSE_LIMITS = {
	sensitivity: { min: 50, max: 1000, step: 50 },
	deadzone: { min: 1, max: 10, step: 1 },
	acceleration: { min: 1, max: 10, step: 1, neutral: 1.0 },
	tapThreshold: { min: 0.02, max: 4, step: 0.01 },
	clickDebounce: { min: 50, max: 600, step: 10 },
	doubleClickWindow: { min: 200, max: 1000, step: 10 }
} as const;

export const AIR_MOUSE_WS = {
	maxHistory: 100,
	watchdogTimeoutMs: 15000,
	reconnectDelayMs: 3000
} as const;
