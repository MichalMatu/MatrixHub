import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import type { WifiSensingApiService } from '$lib/services/api/connectivity/WifiSensingApiService';
import type {
	CsiAlarmBand,
	CsiAlarmSettings,
	CsiMotionStatus
} from '$lib/types/connectivity/wifiSensing';
import { toUserRequestErrorMessage } from '$lib/utils';
import * as m from '$lib/paraglide/messages.js';

const DEFAULT_CSI_ALARM_SETTINGS: CsiAlarmSettings = {
	enabled: false,
	bands: [],
	baseline_frames: 150,
	top_k: 8,
	enter_threshold: 6,
	clear_threshold: 3,
	hold_ms: 1200,
	clear_hold_ms: 2500,
	min_noise: 4,
	min_energy: 4,
	noisy_threshold: 80,
	auto_recalibration: true,
	sensitivity: 1
};

function clamp(value: number, min: number, max: number) {
	if (!Number.isFinite(value)) return min;
	return Math.min(max, Math.max(min, value));
}

function normalizeSensitivity(value: unknown): 0 | 1 | 2 {
	return Math.round(clamp(Number(value), 0, 2)) as 0 | 1 | 2;
}

function sensitivityThresholds(sensitivity: 0 | 1 | 2) {
	if (sensitivity === 0) {
		return { enter: 8, clear: 4 };
	}
	if (sensitivity === 2) {
		return { enter: 4.5, clear: 2.2 };
	}
	return { enter: 6, clear: 3 };
}

function cloneSettings(settings: CsiAlarmSettings): CsiAlarmSettings {
	return {
		...settings,
		bands: settings.bands.map((band) => ({ ...band })),
		sensitivity: normalizeSensitivity(settings.sensitivity)
	};
}

function normalizeBand(band: CsiAlarmBand): CsiAlarmBand {
	const start = Math.round(clamp(band.start, 0, 255));
	const end = Math.round(clamp(band.end, 0, 255));
	return start <= end ? { start, end } : { start: end, end: start };
}

export function normalizeCsiAlarmSettings(settings?: Partial<CsiAlarmSettings>): CsiAlarmSettings {
	const merged = { ...DEFAULT_CSI_ALARM_SETTINGS, ...(settings ?? {}) };
	const sensitivity = normalizeSensitivity(merged.sensitivity);
	const { enter, clear } = sensitivityThresholds(sensitivity);
	const bands = Array.isArray(merged.bands) ? merged.bands.slice(0, 4).map(normalizeBand) : [];

	return {
		enabled: !!merged.enabled,
		bands,
		baseline_frames: Math.round(clamp(Number(merged.baseline_frames), 30, 1000)),
		top_k: Math.round(clamp(Number(merged.top_k), 1, 32)),
		enter_threshold: enter,
		clear_threshold: clear,
		hold_ms: Math.round(clamp(Number(merged.hold_ms), 100, 10000)),
		clear_hold_ms: Math.round(clamp(Number(merged.clear_hold_ms), 100, 30000)),
		min_noise: clamp(Number(merged.min_noise), 0.1, 1000),
		min_energy: clamp(Number(merged.min_energy), 0, 10000),
		noisy_threshold: clamp(Number(merged.noisy_threshold), enter, 500),
		auto_recalibration: !!merged.auto_recalibration,
		sensitivity
	};
}

function applySensitivity(settings: CsiAlarmSettings, sensitivity: 0 | 1 | 2) {
	settings.sensitivity = sensitivity;
	const thresholds = sensitivityThresholds(sensitivity);
	settings.enter_threshold = thresholds.enter;
	settings.clear_threshold = thresholds.clear;
	settings.noisy_threshold = clamp(settings.noisy_threshold, thresholds.enter, 500);
}

export function useCsiAlarmConfig(getApi: () => WifiSensingApiService) {
	const session = useSessionAccess();
	let settings = $state<CsiAlarmSettings>(cloneSettings(DEFAULT_CSI_ALARM_SETTINGS));
	let savedSettings = $state<CsiAlarmSettings | null>(null);
	let motionStatus = $state<CsiMotionStatus | null>(null);
	let loading = $state(false);
	let saving = $state(false);
	let calibrating = $state(false);
	let error = $state<string | null>(null);
	let statusTimer: ReturnType<typeof setInterval> | undefined;
	const isAdmin = $derived(session.canManage);
	const hasChanges = $derived(
		JSON.stringify(normalizeCsiAlarmSettings(settings)) !==
			JSON.stringify(savedSettings ? normalizeCsiAlarmSettings(savedSettings) : null)
	);

	function setSettings(next: CsiAlarmSettings) {
		settings = cloneSettings(normalizeCsiAlarmSettings(next));
	}

	async function loadSettings() {
		loading = true;
		try {
			const loaded = await getApi().getSettings();
			const normalized = normalizeCsiAlarmSettings(loaded.csi_alarm);
			setSettings(normalized);
			savedSettings = cloneSettings(normalized);
			error = null;
		} catch (err) {
			error = toUserRequestErrorMessage(err, {
				timeoutMessage: m.toast_wifisensing_settings_load_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.toast_wifisensing_settings_load_failed({ locale: i18n.languageTag })
			});
		} finally {
			loading = false;
		}
	}

	async function refreshStatus() {
		try {
			const status = await getApi().getStatus();
			motionStatus = status.csi?.motion ?? null;
		} catch {
			motionStatus = null;
		}
	}

	function startStatusPolling() {
		if (statusTimer) return;
		void refreshStatus();
		statusTimer = setInterval(() => void refreshStatus(), 2500);
	}

	function stopStatusPolling() {
		if (statusTimer) {
			clearInterval(statusTimer);
			statusTimer = undefined;
		}
	}

	async function save() {
		if (!isAdmin || saving) return;
		saving = true;
		try {
			const normalized = normalizeCsiAlarmSettings(settings);
			const saved = await getApi().saveSettings({ csi_alarm: normalized });
			const next = normalizeCsiAlarmSettings(saved.csi_alarm);
			setSettings(next);
			savedSettings = cloneSettings(next);
			notifications.success(m.settings_saved({ locale: i18n.languageTag }), 3000);
			error = null;
			await refreshStatus();
		} catch (err) {
			const message = toUserRequestErrorMessage(err, {
				timeoutMessage: m.toast_wifisensing_settings_save_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.toast_wifisensing_settings_save_failed({ locale: i18n.languageTag })
			});
			error = message;
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		} finally {
			saving = false;
		}
	}

	async function calibrate() {
		if (!isAdmin || calibrating) return;
		calibrating = true;
		try {
			const response = await getApi().calibrateCsiAlarm();
			if (!response.ok) {
				throw new Error(response.error ?? 'csi_calibration_failed');
			}
			await refreshStatus();
		} catch (err) {
			const message = toUserRequestErrorMessage(err, {
				timeoutMessage: m.toast_wifisensing_settings_save_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.toast_wifisensing_settings_save_failed({ locale: i18n.languageTag })
			});
			error = message;
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		} finally {
			calibrating = false;
		}
	}

	function addBand(band?: CsiAlarmBand) {
		const next = normalizeBand(band ?? { start: 58, end: 70 });
		if (settings.bands.length >= 4) {
			settings.bands[3] = next;
			return;
		}
		settings.bands.push(next);
	}

	function removeBand(index: number) {
		if (index < 0 || index >= settings.bands.length) return;
		settings.bands.splice(index, 1);
	}

	function setBandFromSelection(band: CsiAlarmBand) {
		addBand(band);
	}

	function setSensitivity(value: 0 | 1 | 2) {
		applySensitivity(settings, value);
	}

	function reset() {
		setSettings(savedSettings ?? DEFAULT_CSI_ALARM_SETTINGS);
	}

	function destroy() {
		stopStatusPolling();
	}

	return {
		get settings() {
			return settings;
		},
		get savedSettings() {
			return savedSettings;
		},
		get motionStatus() {
			return motionStatus;
		},
		get loading() {
			return loading;
		},
		get saving() {
			return saving;
		},
		get calibrating() {
			return calibrating;
		},
		get error() {
			return error;
		},
		get isAdmin() {
			return isAdmin;
		},
		get hasChanges() {
			return hasChanges;
		},
		loadSettings,
		refreshStatus,
		startStatusPolling,
		stopStatusPolling,
		save,
		calibrate,
		addBand,
		removeBand,
		setBandFromSelection,
		setSensitivity,
		reset,
		destroy
	};
}
