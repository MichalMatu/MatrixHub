import type { AlarmRule } from '$lib/types/domain/alarms';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { getAlarmSourceLabel } from '$lib/features/alarms/alarmLabels';
import { useAlarmRulesSource } from '$lib/features/alarms/useAlarmRulesSource.svelte';

export function useAlarmsWidgetManagement() {
	const source = useAlarmRulesSource({ loadFromApiOnStart: false });

	const hasRules = $derived(source.rules.length > 0);
	const hasActiveAlarms = $derived(source.rules.some((rule) => rule.triggered));
	const canManage = $derived(source.canManage);

	async function toggleRule(rule: AlarmRule, event: Event) {
		event.stopPropagation();
		if (!canManage) return;

		const success = await source.toggleRule(rule);
		if (!success && source.errorMessage) {
			notifications.error(
				m.toast_message({ message: source.errorMessage }, { locale: i18n.languageTag }),
				4000
			);
		}
	}

	return {
		get rules() {
			return source.rules;
		},
		get loading() {
			return source.loading;
		},
		get errorMessage() {
			return source.errorMessage;
		},
		get hasRules() {
			return hasRules;
		},
		get hasActiveAlarms() {
			return hasActiveAlarms;
		},
		get canManage() {
			return canManage;
		},
		toggleRule,
		getSourceLabel: getAlarmSourceLabel
	};
}
