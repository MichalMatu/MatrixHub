import { onDestroy, onMount } from 'svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { AlarmsApiService } from '$lib/services/api/monitoring/AlarmsApiService';
import { Logger } from '$lib/services/core/Logger';
import type { AlarmRule, AlarmRulesConfig } from '$lib/types/domain/alarms';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { alarmsStore, type AlarmsStore } from '$lib/stores/alarms.svelte';

type AlarmRulesSourceDeps = {
	access?: Pick<SessionAccess, 'canRead' | 'canManage'>;
	createApi?: () => AlarmsApiService;
	store?: AlarmsStore;
	now?: () => number;
	loadFromApiOnStart?: boolean;
};

type SaveRulesOptions = {
	successMessage?: string;
	rollbackRules?: AlarmRule[];
	errorFallbackMessage?: string;
};

export function useAlarmRulesSource(deps: AlarmRulesSourceDeps = {}) {
	const access = deps.access ?? useSessionAccess();
	const apiClient = deps.createApi ? null : useApiClient();
	const store = deps.store ?? alarmsStore;
	const createApi = () => deps.createApi?.() ?? apiClient!.createService(AlarmsApiService);
	const now = deps.now ?? (() => Math.floor(Date.now() / 1000));
	const loadFromApiOnStart = deps.loadFromApiOnStart ?? true;
	let started = false;

	function resolveSaveErrorMessage(error: unknown, fallbackMessage: string) {
		return toUserRequestErrorMessage(error, {
			timeoutMessage: m.alarms_error_save_timeout({ locale: i18n.languageTag }),
			fallbackMessage
		});
	}

	async function refreshRules(
		options: {
			showLoading?: boolean;
			clearRulesOnError?: boolean;
		} = {}
	) {
		const { showLoading = false, clearRulesOnError = false } = options;

		if (!access.canRead) {
			store.setLoading(false);
			return false;
		}

		if (showLoading) {
			store.setLoading(true);
		}

		try {
			const data = await createApi().getRules({ includeStatus: true });
			store.applySnapshot(data);
			return true;
		} catch (error) {
			Logger.error('Failed to load alarm rules:', error);
			if (getRequestAbortKind(error) === 'abort') return false;
			store.setError(
				toUserRequestErrorMessage(error, {
					timeoutMessage: m.alarms_error_load_timeout({ locale: i18n.languageTag }),
					fallbackMessage: m.alarms_error_load_fallback({ locale: i18n.languageTag })
				})
			);
			if (clearRulesOnError) {
				store.clearRules();
			}
			return false;
		}
	}

	function start() {
		if (!access.canRead || started) {
			if (!access.canRead) {
				store.setLoading(false);
			}
			return;
		}

		const hasCachedSnapshot = store.start();
		started = true;

		if (!hasCachedSnapshot && store.loading && loadFromApiOnStart) {
			void refreshRules({ showLoading: true, clearRulesOnError: true });
		}
	}

	function stop() {
		if (!started) return;
		store.stop();
		started = false;
	}

	onMount(() => {
		start();
	});

	onDestroy(() => {
		stop();
	});

	async function saveRulesSnapshot(nextRules: AlarmRule[], options: SaveRulesOptions = {}) {
		if (!access.canManage) return false;

		const { successMessage, rollbackRules, errorFallbackMessage } = options;
		try {
			const payload: AlarmRulesConfig = { schema_version: 1, rules: nextRules };
			const saved = await createApi().saveRules(payload);
			store.setRules(saved.rules ?? []);
			if (successMessage) {
				notifications.success(successMessage, 3000);
			}
			return true;
		} catch (error) {
			Logger.error('Failed to save alarm rules:', error);
			if (getRequestAbortKind(error) === 'abort') {
				if (rollbackRules) {
					store.setRules(rollbackRules);
				}
				return false;
			}
			const message = resolveSaveErrorMessage(
				error,
				errorFallbackMessage ?? m.alarms_error_save_fallback({ locale: i18n.languageTag })
			);
			store.setError(message);
			const refreshed = await refreshRules();
			if (!refreshed && rollbackRules) {
				store.setRules(rollbackRules);
				store.setError(message);
			}
			return false;
		}
	}

	async function toggleRule(rule: AlarmRule) {
		if (!access.canManage) return false;

		const previousRules = store.rules;
		const optimisticRules = store.rules.map((candidate) =>
			candidate.id === rule.id
				? { ...candidate, enabled: !candidate.enabled, updated_at: now() }
				: candidate
		);
		store.setRules(optimisticRules);

		return saveRulesSnapshot(optimisticRules, {
			rollbackRules: previousRules,
			errorFallbackMessage: m.alarms_error_update({ locale: i18n.languageTag })
		});
	}

	return {
		get rules() {
			return store.rules;
		},
		get loading() {
			return store.loading;
		},
		get errorMessage() {
			return store.errorMessage;
		},
		get canManage() {
			return access.canManage;
		},
		get canRead() {
			return access.canRead;
		},
		refreshRules,
		saveRulesSnapshot,
		toggleRule,
		start,
		stop
	};
}
