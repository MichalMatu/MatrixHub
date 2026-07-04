import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { getBrowserTime } from './components/ntpFormUtils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { NtpApiService } from '$lib/services/api/connectivity/NtpApiService';
import { Logger } from '$lib/services/core/Logger';
import type { NTPSettings, NTPStatus } from '$lib/types/connectivity/ntp';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import type { SettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';

const NOTIFICATION_DURATION_MS = 3000;

const DEFAULT_SETTINGS: NTPSettings = {
	enabled: true,
	server: 'time.google.com',
	tz_label: 'Europe/London',
	tz_format: 'GMT0BST,M3.5.0/1,M10.5.0'
};

type NtpManagementDeps = {
	api?: Pick<NtpApiService, 'getStatus' | 'getSettings' | 'saveSettings' | 'setTime'>;
	session?: Pick<SessionAccess, 'canManage'>;
	notifications?: Pick<typeof notifications, 'success' | 'error'>;
	logger?: Pick<typeof Logger, 'error'>;
	getBrowserTime?: () => string;
	shouldLoad?: () => boolean;
};

export function useNtpManagement(deps: NtpManagementDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const session = deps.session ?? useSessionAccess();
	const toast = deps.notifications ?? notifications;
	const logger = deps.logger ?? Logger;
	const resolveBrowserTime = deps.getBrowserTime ?? getBrowserTime;

	function createApi() {
		return deps.api ?? apiClient!.createService(NtpApiService);
	}

	function handleActionError(error: unknown, fallbackMessage: string) {
		if (getRequestAbortKind(error) === 'abort') return;
		logger.error(`${fallbackMessage}:`, error);
		const message = toUserRequestErrorMessage(error, { fallbackMessage });
		toast.error(
			m.toast_message({ message }, { locale: i18n.languageTag }),
			NOTIFICATION_DURATION_MS
		);
	}

	const feedback: SettingsFeedback<NTPSettings> = {
		resolveErrorMessage: (error, context) =>
			toUserRequestErrorMessage(error, {
				fallbackMessage:
					context === 'save'
						? m.ntp_error_save({ locale: i18n.languageTag })
						: m.settings_load_error({ locale: i18n.languageTag })
			}),
		onError: ({ message, context }) => {
			if (context !== 'save') return;
			toast.error(
				m.toast_message({ message }, { locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		},
		onSaveSuccess: () => {
			toast.success(m.ntp_msg_save_success({ locale: i18n.languageTag }), NOTIFICATION_DURATION_MS);
		}
	};

	const settingsState = useSettings<NTPSettings, Record<string, never>>(
		DEFAULT_SETTINGS,
		{},
		{
			load: async () => await createApi().getSettings(),
			save: async (settings) => await createApi().saveSettings(settings),
			feedback
		}
	);

	let status = $state<NTPStatus | null>(null);
	let statusError = $state<string | null>(null);
	let manualTimeInput = $state('');

	$effect(() => {
		if (settingsState.savedSettings && !settingsState.settings.enabled && !manualTimeInput) {
			manualTimeInput = resolveBrowserTime();
		}
	});

	async function refreshStatus() {
		try {
			status = await createApi().getStatus();
			statusError = null;
		} catch (error) {
			if (getRequestAbortKind(error) === 'abort') return;
			logger.error('Failed to load NTP status:', error);
			status = null;
			statusError = toUserRequestErrorMessage(error);
		}
	}

	async function applyManualTimeIfNeeded() {
		if (settingsState.settings.enabled || !manualTimeInput) return;
		try {
			await createApi().setTime(manualTimeInput);
			// Manual time correction no longer invalidates the current session now
			// that auth is not tied to exp-based clock assumptions.
			toast.success(
				m.ntp_msg_manual_success({ locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		} catch (error) {
			handleActionError(error, m.ntp_error_manual_time({ locale: i18n.languageTag }));
		}
	}

	async function load() {
		await refreshStatus();
		if (!session.canManage) {
			settingsState.loading = false;
			return;
		}
		if (!status) return;
		await settingsState.loadSettings();
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
		void load();
	});

	async function saveSettings() {
		if (!settingsState.savedSettings) return false;
		const saved = await settingsState.saveSettingsNow();
		if (!saved) return false;
		await applyManualTimeIfNeeded();
		await refreshStatus();
		return true;
	}

	function resetSettings() {
		settingsState.resetSettings();
		manualTimeInput = settingsState.settings.enabled ? '' : resolveBrowserTime();
	}

	return {
		get status() {
			return status;
		},
		get statusError() {
			return statusError;
		},
		get settings() {
			return settingsState.settings;
		},
		set settings(nextSettings: NTPSettings) {
			settingsState.setSettings(nextSettings);
		},
		get settingsLoaded() {
			return settingsState.savedSettings !== null;
		},
		get settingsError() {
			return settingsState.errorMessage;
		},
		get manualTimeInput() {
			return manualTimeInput;
		},
		set manualTimeInput(value: string) {
			manualTimeInput = value;
		},
		get isDirty() {
			return settingsState.hasChanges;
		},
		load,
		refreshStatus,
		saveSettings,
		resetSettings
	};
}
