import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';
import { AIR_MOUSE_JIGGLER_DEFAULTS, AIR_MOUSE_JIGGLER_MODES } from './airMouseConfig';

interface AirMouseJigglerNotifications {
	success(message: string, duration?: number): void;
	error(message: string, duration?: number): void;
}

interface AirMouseJigglerFormState {
	status: AirMouseStatus | null;
	saveSettings(settings: Partial<AirMouseStatus>, notify?: boolean): Promise<boolean>;
	fetchStatus(showLoading?: boolean): Promise<void>;
}

interface AirMouseJigglerFormDeps {
	notifications?: AirMouseJigglerNotifications;
}

interface AirMouseJigglerLocalSettings {
	enabled: boolean;
	mode: number;
	interval: number;
	distance: number;
	random: boolean;
}

function syncLocalSettings(
	target: AirMouseJigglerLocalSettings,
	source: AirMouseJigglerLocalSettings
) {
	target.enabled = source.enabled;
	target.mode = source.mode;
	target.interval = source.interval;
	target.distance = source.distance;
	target.random = source.random;
}

function createLocalSettings(
	status: AirMouseStatus | null | undefined
): AirMouseJigglerLocalSettings {
	const jiggler = status?.jiggler;
	if (!jiggler) {
		return {
			enabled: false,
			mode: AIR_MOUSE_JIGGLER_MODES.STEALTH,
			interval: AIR_MOUSE_JIGGLER_DEFAULTS.interval,
			distance: AIR_MOUSE_JIGGLER_DEFAULTS.distance,
			random: AIR_MOUSE_JIGGLER_DEFAULTS.random
		};
	}

	return {
		enabled: jiggler.mode > 0,
		mode: jiggler.mode > 0 ? jiggler.mode : AIR_MOUSE_JIGGLER_MODES.STEALTH,
		interval: jiggler.interval,
		distance: jiggler.move_distance,
		random: jiggler.random_interval
	};
}

function settingsMatchStatus(
	settings: AirMouseJigglerLocalSettings,
	status: AirMouseStatus | null | undefined
) {
	const jiggler = status?.jiggler;
	if (!jiggler) return false;

	const serverEnabled = jiggler.mode > 0;
	const serverMode = serverEnabled ? jiggler.mode : AIR_MOUSE_JIGGLER_MODES.STEALTH;

	return (
		settings.enabled === serverEnabled &&
		settings.mode === serverMode &&
		settings.interval === jiggler.interval &&
		settings.distance === jiggler.move_distance &&
		settings.random === jiggler.random_interval
	);
}

export function useAirMouseJigglerForm(
	getMouseState: () => AirMouseJigglerFormState,
	deps: AirMouseJigglerFormDeps = {}
) {
	const toast = deps.notifications ?? notifications;
	const settings = $state<AirMouseJigglerLocalSettings>(
		createLocalSettings(getMouseState().status)
	);

	let saving = $state(false);
	let settingsSynced = $state(false);
	let forceStatusSync = $state(false);

	const hasChanges = $derived.by(() => !settingsMatchStatus(settings, getMouseState().status));
	const requiresRestart = $derived.by(() => {
		const status = getMouseState().status;
		if (!status?.jiggler) return false;

		return settings.enabled !== status.jiggler.mode > 0;
	});

	$effect(() => {
		const status = getMouseState().status;
		if (!status?.jiggler) return;
		if (settingsSynced && hasChanges && !forceStatusSync) return;

		syncLocalSettings(settings, createLocalSettings(status));
		settingsSynced = true;
		forceStatusSync = false;
	});

	async function save() {
		return persistSettings();
	}

	async function persistSettings(throwOnError = false) {
		if (saving || !hasChanges) {
			if (throwOnError) {
				throw new Error('Air Mouse jiggler settings save skipped');
			}
			return false;
		}

		const modeToSave = settings.enabled
			? settings.mode || AIR_MOUSE_JIGGLER_MODES.STEALTH
			: AIR_MOUSE_JIGGLER_MODES.OFF;

		saving = true;
		try {
			const success = await getMouseState().saveSettings(
				{
					jiggler: {
						mode: modeToSave,
						interval: settings.interval,
						move_distance: settings.distance,
						random_interval: settings.random
					}
				},
				false
			);

			if (!success) {
				if (throwOnError) {
					throw new Error('Failed to save Air Mouse jiggler settings');
				}
				return false;
			}

			forceStatusSync = true;
			await getMouseState().fetchStatus();
			toast.success(m.jiggler_msg_saved(), 3000);
			return true;
		} catch (error) {
			console.error(error);
			toast.error(m.settings_save_error(), 5000);
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
		if (!status?.jiggler) return;
		syncLocalSettings(settings, createLocalSettings(status));
		settingsSynced = true;
		forceStatusSync = false;
	}

	return {
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
	};
}
