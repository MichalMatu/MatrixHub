/**
 * @file useUdpSettings.svelte.ts
 * @brief Composable hook for UDP data pusher settings
 */

import {
	UdpApiService,
	type UdpSettings,
	type UdpFormat
} from '$lib/services/api/integrations/UdpApiService';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import type { SettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

const DEFAULT_SETTINGS: UdpSettings = {
	enabled: false,
	host: '',
	port: 8089,
	format: 'line',
	interval_ms: 60000
};

const VALID_FORMATS: UdpFormat[] = ['line', 'json', 'csv'];
const MIN_INTERVAL_MS = 10000;
const MIN_PORT = 1;
const MAX_PORT = 65535;

interface UdpErrors {
	host: boolean;
	port: boolean;
}

const DEFAULT_ERRORS: UdpErrors = {
	host: false,
	port: false
};

type UdpSettingsDeps = {
	api?: Pick<UdpApiService, 'getSettings' | 'updateSettings'>;
	feedback?: SettingsFeedback<UdpSettings>;
	shouldLoad?: () => boolean;
};

function filterAsciiPrintable(value: string): string {
	// Keep only printable ASCII (0x20..0x7E)
	return value.replace(/[^\x20-\x7E]/g, '');
}

function sanitizeHost(value: string): string {
	return filterAsciiPrintable(value).replace(/\s+/g, '');
}

function isValidFormat(value: string): value is UdpFormat {
	return VALID_FORMATS.includes(value as UdpFormat);
}

function normalizeLoadedSettings(data: UdpSettings): UdpSettings {
	return {
		enabled: Boolean(data.enabled),
		host: sanitizeHost(data.host ?? ''),
		port:
			Number.isFinite(data.port) && data.port >= MIN_PORT && data.port <= MAX_PORT
				? Math.trunc(data.port)
				: DEFAULT_SETTINGS.port,
		format: isValidFormat(data.format) ? data.format : DEFAULT_SETTINGS.format,
		interval_ms:
			Number.isFinite(data.interval_ms) && data.interval_ms >= MIN_INTERVAL_MS
				? Math.trunc(data.interval_ms)
				: DEFAULT_SETTINGS.interval_ms
	};
}

function sanitizeSettingsForSave(settings: UdpSettings): UdpSettings {
	return {
		...settings,
		host: sanitizeHost(settings.host)
	};
}

export function useUdpSettings(deps: UdpSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const feedback = deps.feedback ?? createSettingsFeedback<UdpSettings>();

	function createApi() {
		return deps.api ?? apiClient!.createService(UdpApiService);
	}

	function isValidPort(value: number): boolean {
		return Number.isFinite(value) && value >= MIN_PORT && value <= MAX_PORT;
	}

	// UDP does NOT require a restart confirmation, so we omit the 'restart' config.
	const hook = useSettings<UdpSettings, UdpErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
		load: async () => {
			return normalizeLoadedSettings(await createApi().getSettings());
		},
		save: async (settings) => {
			return await createApi().updateSettings(sanitizeSettingsForSave(settings));
		},
		validate: (settings, errors) => {
			let hasError = false;
			if (sanitizeHost(settings.host) === '') {
				errors.host = true;
				hasError = true;
			}
			if (!isValidPort(settings.port)) {
				errors.port = true;
				hasError = true;
			}
			return hasError;
		},
		shouldValidate: (settings) => settings.enabled,
		feedback
		// No restart config -> direct save
	});

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
		void hook.loadSettings();
	});

	let canTest = $derived.by(() => {
		const savedSettings = hook.savedSettings;
		return (
			savedSettings !== null &&
			savedSettings.enabled &&
			sanitizeHost(savedSettings.host) !== '' &&
			isValidPort(savedSettings.port)
		);
	});

	function setFormat(format: UdpFormat) {
		if (isValidFormat(format)) {
			hook.updateSetting('format', format);
		}
	}

	function setInterval(ms: number) {
		if (Number.isFinite(ms) && ms >= MIN_INTERVAL_MS) {
			hook.updateSetting('interval_ms', Math.trunc(ms));
		}
	}

	return {
		get settings() {
			return hook.settings;
		},
		get savedSettings() {
			return hook.savedSettings;
		},
		get loading() {
			return hook.loading;
		},
		set loading(v: boolean) {
			hook.loading = v;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		get errors() {
			return hook.errors;
		},
		get errorMessage() {
			return hook.errorMessage;
		},
		loadSettings: hook.loadSettings,
		saveSettings: hook.saveSettings,
		resetSettings: hook.resetSettings,
		updateSetting: (key: keyof UdpSettings, value: UdpSettings[keyof UdpSettings]) => {
			if (key === 'host' && typeof value === 'string') {
				hook.updateSetting(key, sanitizeHost(value));
				return;
			}
			if (key === 'port' && typeof value === 'number' && Number.isFinite(value)) {
				hook.updateSetting(key, Math.trunc(value) as UdpSettings[typeof key]);
				return;
			}
			hook.updateSetting(key, value);
		},
		setFormat,
		setInterval,
		// Specifics
		get canTest() {
			return canTest;
		}
	};
}
