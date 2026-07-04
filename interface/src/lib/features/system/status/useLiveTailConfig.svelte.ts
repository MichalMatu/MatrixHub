import type { LoggingConfig, SystemApiService } from '$lib/services/api/core/SystemApiService';
import { Logger } from '$lib/services/core/Logger';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

const VALID_LEVELS = ['none', 'error', 'warn', 'info', 'debug', 'verbose'] as const;

export function useLiveTailConfig(getApi: () => SystemApiService) {
	let loggingConfig = $state<LoggingConfig>({ level: '' });
	let originalLoggingConfig = $state<LoggingConfig>({ level: '' });
	let isLoggingConfigLoaded = $state(false);
	let savingConfig = $state(false);
	let error = $state<string | null>(null);

	const levels = [...VALID_LEVELS];
	const isDirty = $derived(
		isLoggingConfigLoaded && loggingConfig.level !== originalLoggingConfig.level
	);

	async function refreshLoggingConfig() {
		try {
			const config = await getApi().getConfig();
			applyLoggingLevel(config.logging?.level);
			error = null;
		} catch (nextError) {
			Logger.error('Failed to load logging config:', nextError);
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			error = toUserRequestErrorMessage(nextError);
		}
	}

	async function saveLoggingSettings() {
		savingConfig = true;
		const levelToSave = loggingConfig.level;
		try {
			await getApi().saveConfig({ logging: { level: levelToSave } });
			originalLoggingConfig = { level: levelToSave };
			loggingConfig = { level: levelToSave };
			error = null;
		} catch (nextError) {
			Logger.error('Failed to save logging config:', nextError);
			const kind = getRequestAbortKind(nextError);
			if (kind === 'abort') return;
			error = toUserRequestErrorMessage(nextError, {
				fallbackMessage: m.livetail_error_save({ locale: i18n.languageTag })
			});
		} finally {
			savingConfig = false;
		}
	}

	function resetLoggingSettings() {
		loggingConfig = { level: originalLoggingConfig.level };
		error = null;
	}

	function applyLoggingLevel(level: string | undefined) {
		const normalized = normalizeLoggingLevel(level);
		const shouldKeepLocalDraft =
			isLoggingConfigLoaded && loggingConfig.level !== originalLoggingConfig.level;
		originalLoggingConfig = { level: normalized };
		if (!shouldKeepLocalDraft) {
			loggingConfig = { level: normalized };
		}
		isLoggingConfigLoaded = true;
	}

	function normalizeLoggingLevel(level: string | undefined): string {
		if (level && VALID_LEVELS.includes(level as (typeof VALID_LEVELS)[number])) {
			return level;
		}
		return 'info';
	}

	return {
		get loggingConfig() {
			return loggingConfig;
		},
		get originalLoggingConfig() {
			return originalLoggingConfig;
		},
		get isLoggingConfigLoaded() {
			return isLoggingConfigLoaded;
		},
		get savingConfig() {
			return savingConfig;
		},
		get error() {
			return error;
		},
		get isDirty() {
			return isDirty;
		},
		get levels() {
			return levels;
		},
		refreshLoggingConfig,
		saveLoggingSettings,
		resetLoggingSettings
	};
}
