import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { UsbTerminalApiService } from '$lib/services/api/integrations/UsbTerminalApiService';
import type { UsbTerminalData } from '$lib/types/connectivity/usbTerminal';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';

const DEFAULT_IDLE_TIMEOUT_MS = 2000;
const MIN_IDLE_TIMEOUT_MS = 500;
const MAX_IDLE_TIMEOUT_MS = 30000;
const MAX_TARGET_PORT_LEN = 31;
const NOTIFICATION_DURATION_MS = 3000;
const AUTO_TARGET_PORT = 'auto';

type UsbTerminalApi = Pick<UsbTerminalApiService, 'getConfig' | 'updateConfig'>;

interface UsbTerminalNotifications {
	error(message: string, duration?: number): void;
	success(message: string, duration?: number): void;
	warning(message: string, duration?: number): void;
}

interface UsbTerminalUiState {
	enabled: boolean;
	idle_timeout_ms: number | '';
	target_port: string;
}

interface UsbTerminalAdvancedDraft {
	idle_timeout_ms: number | '';
	target_port: string;
}

interface UsbTerminalErrors {
	idle_timeout_ms: boolean;
	target_port: boolean;
}

interface UsbTerminalDeps {
	createApi?: () => UsbTerminalApi;
	notifications?: UsbTerminalNotifications;
	shouldLoad?: () => boolean;
}

function createDefaultState(): UsbTerminalUiState {
	return {
		enabled: false,
		idle_timeout_ms: DEFAULT_IDLE_TIMEOUT_MS,
		target_port: AUTO_TARGET_PORT
	};
}

function createDefaultErrors(): UsbTerminalErrors {
	return {
		idle_timeout_ms: false,
		target_port: false
	};
}

function localizeSettingsErrorMessage(message: string): string {
	if (message === 'usb_terminal/session_active') {
		return m.usb_terminal_disable_requires_cancel({ locale: i18n.languageTag });
	}

	return message;
}

function filterAsciiPrintable(value: string): string {
	return value.replace(/[^\x20-\x7E]/g, '');
}

function sanitizeTargetPort(value: string): string {
	return filterAsciiPrintable(value).slice(0, MAX_TARGET_PORT_LEN);
}

function normalizeTargetPort(value: string): string {
	const sanitized = sanitizeTargetPort(value).trim();
	if (!sanitized.length) return AUTO_TARGET_PORT;
	return sanitized.toLowerCase() === AUTO_TARGET_PORT ? AUTO_TARGET_PORT : sanitized;
}

function parseIdleTimeoutValue(value: number | string | ''): number | '' {
	if (typeof value === 'number') {
		return Number.isFinite(value) ? value : '';
	}

	const normalized = value.trim();
	if (!normalized.length) return '';

	const parsed = Number(normalized);
	return Number.isFinite(parsed) ? parsed : '';
}

function clampIdleTimeout(value: number): number {
	return Math.min(MAX_IDLE_TIMEOUT_MS, Math.max(MIN_IDLE_TIMEOUT_MS, Math.round(value)));
}

function toPersistedSettings(settings: UsbTerminalUiState | UsbTerminalData): UsbTerminalData {
	const parsedIdleTimeout = parseIdleTimeoutValue(settings.idle_timeout_ms);

	return {
		enabled: Boolean(settings.enabled),
		idle_timeout_ms: clampIdleTimeout(
			parsedIdleTimeout === '' ? DEFAULT_IDLE_TIMEOUT_MS : parsedIdleTimeout
		),
		target_port: normalizeTargetPort(settings.target_port)
	};
}

function toAdvancedDraft(settings: UsbTerminalData): UsbTerminalAdvancedDraft {
	const normalized = toPersistedSettings(settings);
	return {
		idle_timeout_ms: normalized.idle_timeout_ms,
		target_port: normalized.target_port
	};
}

function clonePersistedSettings(settings: UsbTerminalData): UsbTerminalData {
	return {
		...settings
	};
}

export function useUsbTerminalSettings(deps: UsbTerminalDeps = {}) {
	const apiClient = deps.createApi ? null : useApiClient();
	const toast = deps.notifications ?? notifications;
	const defaultState = createDefaultState();

	let enabled = $state(defaultState.enabled);
	let advancedSettings = $state<UsbTerminalAdvancedDraft>({
		idle_timeout_ms: defaultState.idle_timeout_ms,
		target_port: defaultState.target_port
	});
	let savedSettings = $state<UsbTerminalData | null>(null);
	let errors = $state<UsbTerminalErrors>(createDefaultErrors());
	let loading = $state(true);
	let saving = $state(false);
	let error = $state<string | null>(null);

	let hasConfig = $derived(savedSettings !== null);
	let hasEnabledChanges = $derived(savedSettings !== null && enabled !== savedSettings.enabled);
	let hasAdvancedChanges = $derived.by(() => {
		if (!savedSettings) return false;

		const current = toPersistedSettings({
			enabled: savedSettings.enabled,
			...advancedSettings
		});

		return (
			current.idle_timeout_ms !== savedSettings.idle_timeout_ms ||
			current.target_port !== savedSettings.target_port
		);
	});
	let hasChanges = $derived.by(() => {
		return hasEnabledChanges || hasAdvancedChanges;
	});

	function createApi(): UsbTerminalApi {
		if (deps.createApi) {
			return deps.createApi();
		}

		return apiClient!.createService(UsbTerminalApiService);
	}

	function clearErrors() {
		errors.idle_timeout_ms = false;
		errors.target_port = false;
	}

	function setSavedSettings(nextSettings: UsbTerminalData) {
		const normalized = toPersistedSettings(nextSettings);
		savedSettings = clonePersistedSettings(normalized);
		error = null;
		clearErrors();
	}

	function resetEnabledDraft() {
		if (!savedSettings) return;
		enabled = savedSettings.enabled;
	}

	function resetAdvancedDraft() {
		if (!savedSettings) return;
		advancedSettings = toAdvancedDraft(savedSettings);
		clearErrors();
	}

	async function loadSettings(): Promise<void> {
		loading = true;
		error = null;

		try {
			const data = await createApi().getConfig();
			setSavedSettings(data);
			resetEnabledDraft();
			resetAdvancedDraft();
		} catch (err) {
			if (getRequestAbortKind(err) === 'abort') return;
			error = toUserRequestErrorMessage(err, {
				fallbackMessage: m.settings_load_error({ locale: i18n.languageTag })
			});
		} finally {
			loading = false;
		}
	}

	let autoLoadArmed = true;

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (shouldLoad === undefined) return;
		if (!shouldLoad) {
			autoLoadArmed = true;
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;

		void loadSettings();
	});

	function setEnabled(value: boolean) {
		enabled = value;
		if (!value) {
			errors.target_port = false;
		}
	}

	function updateTargetPort(value: string) {
		advancedSettings.target_port = sanitizeTargetPort(value);
		errors.target_port = false;
	}

	function updateIdleTimeout(value: number | string) {
		advancedSettings.idle_timeout_ms = parseIdleTimeoutValue(value);
		errors.idle_timeout_ms = false;
	}

	function normalizeIdleTimeout() {
		const parsed = parseIdleTimeoutValue(advancedSettings.idle_timeout_ms);
		advancedSettings.idle_timeout_ms =
			parsed === '' ? DEFAULT_IDLE_TIMEOUT_MS : clampIdleTimeout(parsed);
		errors.idle_timeout_ms = false;
	}

	function validateSettings(nextSettings: UsbTerminalUiState | UsbTerminalData): boolean {
		clearErrors();
		const normalized = toPersistedSettings(nextSettings);

		let hasValidationError = false;

		if (normalized.enabled && !normalizeTargetPort(normalized.target_port).trim()) {
			errors.target_port = true;
			hasValidationError = true;
		}

		if (parseIdleTimeoutValue(nextSettings.idle_timeout_ms) === '') {
			errors.idle_timeout_ms = true;
			hasValidationError = true;
		}

		if (hasValidationError) {
			toast.warning(
				m.settings_validation_error({ locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
			return false;
		}

		return true;
	}

	function getEnabledPayload(): UsbTerminalData | null {
		if (!savedSettings) return null;

		const payload = toPersistedSettings({
			...savedSettings,
			enabled
		});
		if (!validateSettings(payload)) return null;

		return payload;
	}

	async function persistEnabled(
		options: {
			payload?: UsbTerminalData;
			throwOnError?: boolean;
		} = {}
	): Promise<boolean> {
		const { payload = getEnabledPayload(), throwOnError = false } = options;
		if (!savedSettings || !hasEnabledChanges || saving || !payload) {
			if (throwOnError) {
				throw new Error('USB Terminal settings save skipped');
			}
			return false;
		}

		saving = true;

		try {
			const savedConfig = await createApi().updateConfig(payload);
			setSavedSettings(savedConfig);
			enabled = savedConfig.enabled;
			toast.success(m.settings_saved({ locale: i18n.languageTag }), NOTIFICATION_DURATION_MS);
			return true;
		} catch (err) {
			if (getRequestAbortKind(err) === 'abort') return false;

			const message = localizeSettingsErrorMessage(
				toUserRequestErrorMessage(err, {
					fallbackMessage: m.settings_save_error({ locale: i18n.languageTag })
				})
			);
			toast.error(
				m.toast_message({ message }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
			if (throwOnError) {
				throw err;
			}
			return false;
		} finally {
			saving = false;
		}
	}

	async function saveEnabled(): Promise<boolean> {
		return persistEnabled();
	}

	async function saveEnabledChange(payload: UsbTerminalData): Promise<boolean> {
		try {
			return await persistEnabled({ payload, throwOnError: true });
		} catch (err) {
			resetEnabledDraft();
			throw err;
		}
	}

	function confirmEnabledChange(nextEnabled: boolean) {
		if (!savedSettings || saving) return;

		setEnabled(nextEnabled);

		if (nextEnabled === savedSettings.enabled) return;

		const payload = getEnabledPayload();
		if (!payload) {
			resetEnabledDraft();
			return;
		}

		// The terminal card no longer keeps a separate pending enabled draft.
		// The header toggle is the only entry point, so cancelling the modal
		// or failing the save must restore the persisted enabled state.
		confirmRestartAndSave(() => saveEnabledChange(payload), {
			message: m.restart_confirm_msg_generic({ locale: i18n.languageTag }),
			onCancel: resetEnabledDraft
		});
	}

	async function saveAdvancedSettings(): Promise<boolean> {
		if (!savedSettings || !hasAdvancedChanges || saving) return false;

		const payload = toPersistedSettings({
			enabled: savedSettings.enabled,
			...advancedSettings
		});
		if (!validateSettings({ enabled: savedSettings.enabled, ...advancedSettings })) return false;

		saving = true;

		try {
			const savedConfig = await createApi().updateConfig(payload);
			setSavedSettings(savedConfig);
			advancedSettings = toAdvancedDraft(savedConfig);
			toast.success(m.settings_saved({ locale: i18n.languageTag }), NOTIFICATION_DURATION_MS);
			return true;
		} catch (err) {
			if (getRequestAbortKind(err) === 'abort') return false;

			const message = localizeSettingsErrorMessage(
				toUserRequestErrorMessage(err, {
					fallbackMessage: m.settings_save_error({ locale: i18n.languageTag })
				})
			);
			toast.error(
				m.toast_message({ message }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
			return false;
		} finally {
			saving = false;
		}
	}

	return {
		get settings() {
			return {
				enabled,
				...advancedSettings
			};
		},
		get enabled() {
			return enabled;
		},
		get advancedSettings() {
			return advancedSettings;
		},
		get errors() {
			return errors;
		},
		get loading() {
			return loading;
		},
		set loading(value: boolean) {
			loading = value;
		},
		get saving() {
			return saving;
		},
		get error() {
			return error;
		},
		get hasConfig() {
			return hasConfig;
		},
		get hasChanges() {
			return hasChanges;
		},
		get hasEnabledChanges() {
			return hasEnabledChanges;
		},
		get hasAdvancedChanges() {
			return hasAdvancedChanges;
		},
		loadSettings,
		saveEnabled,
		confirmEnabledChange,
		saveAdvancedSettings,
		setEnabled,
		updateIdleTimeout,
		updateTargetPort,
		normalizeIdleTimeout,
		resetEnabledDraft,
		resetAdvancedDraft
	};
}
