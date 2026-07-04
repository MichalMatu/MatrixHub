import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';
import {
	AIR_MOUSE_LIMITS,
	AIR_MOUSE_LOCAL_DEFAULTS,
	toAirMouseClickAction,
	toAirMouseClickSource,
	type AirMouseLocalSettings
} from './airMouseConfig';

interface AirMouseSettingsFormState {
	status: AirMouseStatus | null;
	saveSettings(settings: Partial<AirMouseStatus>, notify?: boolean): Promise<boolean>;
}

function normalizeScript(value: string | null | undefined) {
	return value ?? '';
}

function syncLocalSettings(target: AirMouseLocalSettings, source: AirMouseLocalSettings) {
	target.movement_enabled = source.movement_enabled;
	target.click_enabled = source.click_enabled;
	target.sensitivityX = source.sensitivityX;
	target.sensitivityY = source.sensitivityY;
	target.deadzone = source.deadzone;
	target.accelerationFactor = source.accelerationFactor;
	target.tapThreshold = source.tapThreshold;
	target.clickDebounce = source.clickDebounce;
	target.doubleClickWindow = source.doubleClickWindow;
	target.click_source = source.click_source;
	target.single_click_action = source.single_click_action;
	target.double_click_action = source.double_click_action;
	target.triple_click_action = source.triple_click_action;
	target.single_click_script = source.single_click_script;
	target.double_click_script = source.double_click_script;
	target.triple_click_script = source.triple_click_script;
}

function createAirMouseLocalSettings(
	status: AirMouseStatus | null | undefined
): AirMouseLocalSettings {
	if (!status) {
		return { ...AIR_MOUSE_LOCAL_DEFAULTS };
	}

	return {
		movement_enabled: status.movement_enabled,
		click_enabled: status.click_enabled,
		sensitivityX: status.sensitivity_x,
		sensitivityY: status.sensitivity_y,
		deadzone: status.deadzone,
		accelerationFactor: status.acceleration_enabled
			? status.acceleration_factor
			: AIR_MOUSE_LIMITS.acceleration.neutral,
		tapThreshold: status.tap_threshold_g,
		clickDebounce: status.click_debounce_ms,
		doubleClickWindow: status.double_click_window_ms,
		click_source: toAirMouseClickSource(status.click_source),
		single_click_action: toAirMouseClickAction(status.single_click_action),
		double_click_action: toAirMouseClickAction(status.double_click_action),
		triple_click_action: toAirMouseClickAction(status.triple_click_action),
		single_click_script: normalizeScript(status.single_click_script),
		double_click_script: normalizeScript(status.double_click_script),
		triple_click_script: normalizeScript(status.triple_click_script)
	};
}

function createAirMouseSettingsPayload(settings: AirMouseLocalSettings): Partial<AirMouseStatus> {
	return {
		movement_enabled: settings.movement_enabled,
		click_enabled: settings.click_enabled,
		sensitivity_x: settings.sensitivityX,
		sensitivity_y: settings.sensitivityY,
		deadzone: settings.deadzone,
		acceleration_enabled: settings.accelerationFactor > AIR_MOUSE_LIMITS.acceleration.neutral,
		acceleration_factor:
			settings.accelerationFactor > AIR_MOUSE_LIMITS.acceleration.neutral
				? settings.accelerationFactor
				: AIR_MOUSE_LIMITS.acceleration.neutral,
		tap_threshold_g: settings.tapThreshold,
		click_debounce_ms: settings.clickDebounce,
		double_click_window_ms: settings.doubleClickWindow,
		click_source: settings.click_source,
		single_click_action: settings.single_click_action,
		double_click_action: settings.double_click_action,
		triple_click_action: settings.triple_click_action,
		single_click_script: settings.single_click_script,
		double_click_script: settings.double_click_script,
		triple_click_script: settings.triple_click_script
	};
}

function airMouseSettingsMatchStatus(
	settings: AirMouseLocalSettings,
	status: AirMouseStatus | null | undefined
) {
	if (!status) return false;

	const serverAccelFactor = status.acceleration_enabled
		? status.acceleration_factor
		: AIR_MOUSE_LIMITS.acceleration.neutral;

	return (
		settings.movement_enabled === status.movement_enabled &&
		settings.click_enabled === status.click_enabled &&
		settings.sensitivityX === status.sensitivity_x &&
		settings.sensitivityY === status.sensitivity_y &&
		settings.deadzone === status.deadzone &&
		settings.accelerationFactor === serverAccelFactor &&
		settings.tapThreshold === status.tap_threshold_g &&
		settings.clickDebounce === status.click_debounce_ms &&
		settings.doubleClickWindow === status.double_click_window_ms &&
		settings.click_source === toAirMouseClickSource(status.click_source) &&
		settings.single_click_action === toAirMouseClickAction(status.single_click_action) &&
		settings.double_click_action === toAirMouseClickAction(status.double_click_action) &&
		settings.triple_click_action === toAirMouseClickAction(status.triple_click_action) &&
		settings.single_click_script === normalizeScript(status.single_click_script) &&
		settings.double_click_script === normalizeScript(status.double_click_script) &&
		settings.triple_click_script === normalizeScript(status.triple_click_script)
	);
}

export function useAirMouseSettingsForm(getMouseState: () => AirMouseSettingsFormState) {
	const settings = $state<AirMouseLocalSettings>({ ...AIR_MOUSE_LOCAL_DEFAULTS });

	let saving = $state(false);
	let settingsSynced = $state(false);
	let forceStatusSync = $state(false);

	const hasChanges = $derived.by(
		() => !airMouseSettingsMatchStatus(settings, getMouseState().status)
	);
	const requiresRestart = $derived.by(() => {
		const status = getMouseState().status;
		if (!status) return false;

		return (
			settings.movement_enabled !== status.movement_enabled ||
			settings.click_enabled !== status.click_enabled
		);
	});

	$effect(() => {
		const status = getMouseState().status;
		if (!status) return;
		if (settingsSynced && hasChanges && !forceStatusSync) return;

		syncLocalSettings(settings, createAirMouseLocalSettings(status));
		settingsSynced = true;
		forceStatusSync = false;
	});

	async function save() {
		return persistSettings();
	}

	async function persistSettings(throwOnError = false) {
		if (saving || !hasChanges) {
			if (throwOnError) {
				throw new Error('Air Mouse settings save skipped');
			}
			return false;
		}

		saving = true;
		try {
			const saved = await getMouseState().saveSettings(createAirMouseSettingsPayload(settings));
			if (saved) {
				forceStatusSync = true;
			}
			if (!saved && throwOnError) {
				throw new Error('Failed to save Air Mouse settings');
			}
			return saved;
		} catch (error) {
			if (throwOnError) {
				throw error;
			}
			return false;
		} finally {
			saving = false;
		}
	}

	function confirmSave() {
		if (!hasChanges) return;

		if (!requiresRestart) {
			void save();
			return;
		}

		confirmRestartAndSave(() => persistSettings(true), {
			message: m.restart_confirm_msg_generic({ locale: i18n.languageTag })
		});
	}

	function reset() {
		const status = getMouseState().status;
		if (!status) return;
		syncLocalSettings(settings, createAirMouseLocalSettings(status));
		settingsSynced = true;
		forceStatusSync = false;
	}

	let form = $state({
		settings,
		get saving() {
			return saving;
		},
		get hasChanges() {
			return hasChanges;
		},
		get requiresRestart() {
			return requiresRestart;
		},
		save,
		confirmSave,
		reset
	});

	return form;
}
