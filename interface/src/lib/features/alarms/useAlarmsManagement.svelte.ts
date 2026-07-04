import { ConfirmDialog } from '$lib/components';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import type { AlarmsApiService } from '$lib/services/api/monitoring/AlarmsApiService';
import type { AlarmRule } from '$lib/types/domain/alarms';
import { MAX_ALARM_RULES } from '$lib/types/domain/alarms';
import { confirm } from '$lib/utils/ui/dialogs';
import type { ModalOpenService } from '$lib/utils/ui/modal';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { useAlarmRulesSource } from './useAlarmRulesSource.svelte';

type AlarmsManagementDeps = {
	access?: Pick<SessionAccess, 'canRead' | 'canManage'>;
	createApi?: () => AlarmsApiService;
	notifications?: Pick<typeof notifications, 'success' | 'error'>;
	modalService?: ModalOpenService;
	confirmDialogComponent?: unknown;
	now?: () => number;
};

export function useAlarmsManagement(deps: AlarmsManagementDeps = {}) {
	const access = deps.access ?? useSessionAccess();
	const toast = deps.notifications ?? notifications;
	const modalService = deps.modalService;
	const confirmDialogComponent = deps.confirmDialogComponent ?? ConfirmDialog;
	const source = useAlarmRulesSource({
		access,
		createApi: deps.createApi,
		now: deps.now
	});

	let showModal = $state(false);
	let showInfoModal = $state(false);
	let modalSaving = $state(false);
	let editingRule = $state<AlarmRule | null>(null);
	let localError = $state<string | null>(null);
	let maxRulesReached = $derived(source.rules.length >= MAX_ALARM_RULES);

	async function loadRules(): Promise<void> {
		const success = await source.refreshRules({
			showLoading: true,
			clearRulesOnError: true
		});

		if (!success && source.errorMessage) {
			toast.error(
				m.error_prefix({ error: source.errorMessage }, { locale: i18n.languageTag }),
				5000
			);
		}
	}

	async function saveRules(
		nextRules: AlarmRule[],
		action: 'save' | 'delete' = 'save'
	): Promise<boolean> {
		if (!access.canManage) return false;

		const success = await source.saveRulesSnapshot(nextRules, {
			successMessage:
				action === 'delete'
					? m.alarms_delete_success({ locale: i18n.languageTag })
					: m.alarms_save_success({ locale: i18n.languageTag })
		});

		if (!success && source.errorMessage) {
			toast.error(
				m.error_prefix({ error: source.errorMessage }, { locale: i18n.languageTag }),
				5000
			);
		}

		return success;
	}

	function openAddModal() {
		if (!access.canManage) return;

		if (maxRulesReached) {
			const message = m.alarms_max_rules_reached(
				{ max: MAX_ALARM_RULES },
				{ locale: i18n.languageTag }
			);
			localError = message;
			toast.error(message, 4000);
			return;
		}

		editingRule = null;
		localError = null;
		showModal = true;
	}

	function openInfoModal() {
		showInfoModal = true;
	}

	function openEditModal(rule: AlarmRule) {
		if (!access.canManage) return;
		localError = null;
		editingRule = rule;
		showModal = true;
	}

	function closeRuleModal() {
		showModal = false;
		editingRule = null;
	}

	function closeInfoModal() {
		showInfoModal = false;
	}

	async function submitRule(rule: AlarmRule): Promise<void> {
		if (!access.canManage) return;

		if (!editingRule && maxRulesReached) {
			const message = m.alarms_max_rules_reached(
				{ max: MAX_ALARM_RULES },
				{ locale: i18n.languageTag }
			);
			localError = message;
			toast.error(message, 4000);
			return;
		}

		const normalizedName = rule.name.trim().toLowerCase();
		const hasDuplicateName = source.rules.some(
			(currentRule) =>
				currentRule.id !== rule.id && currentRule.name.trim().toLowerCase() === normalizedName
		);
		if (hasDuplicateName) {
			const message = m.alarms_error_duplicate_name({ locale: i18n.languageTag });
			localError = message;
			toast.error(message, 4000);
			return;
		}

		modalSaving = true;
		const nextRules = editingRule
			? source.rules.map((currentRule) => (currentRule.id === rule.id ? rule : currentRule))
			: [...source.rules, rule];
		const success = await saveRules(nextRules);
		modalSaving = false;

		if (success) {
			localError = null;
			closeRuleModal();
		}
	}

	function toggleRule(rule: AlarmRule) {
		if (!access.canManage) return;
		void source.toggleRule(rule).then((success) => {
			if (!success && source.errorMessage) {
				toast.error(
					m.error_prefix({ error: source.errorMessage }, { locale: i18n.languageTag }),
					5000
				);
			}
		});
	}

	function confirmDelete(rule: AlarmRule) {
		if (!access.canManage) return;

		confirm({
			title: m.alarms_delete_title({ locale: i18n.languageTag }),
			message: m.alarms_delete_msg({ name: rule.name }, { locale: i18n.languageTag }),
			onConfirm: () => {
				const nextRules = source.rules.filter((currentRule) => currentRule.id !== rule.id);
				void saveRules(nextRules, 'delete');
			},
			component: confirmDialogComponent,
			modalService
		});
	}

	return {
		get rules() {
			return source.rules;
		},
		get loading() {
			return source.loading;
		},
		get showModal() {
			return showModal;
		},
		get showInfoModal() {
			return showInfoModal;
		},
		get modalSaving() {
			return modalSaving;
		},
		get editingRule() {
			return editingRule;
		},
		get error() {
			return localError ?? source.errorMessage;
		},
		get canManage() {
			return access.canManage;
		},
		get canRead() {
			return access.canRead;
		},
		get maxRulesReached() {
			return maxRulesReached;
		},
		loadRules,
		saveRules,
		openAddModal,
		openInfoModal,
		openEditModal,
		closeRuleModal,
		closeInfoModal,
		submitRule,
		toggleRule,
		confirmDelete
	};
}
