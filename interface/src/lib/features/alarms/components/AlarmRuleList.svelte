<script lang="ts">
	import Bell from '~icons/tabler/bell';
	import Plus from '~icons/tabler/plus';
	import Trash from '~icons/tabler/trash';
	import Edit from '~icons/tabler/edit';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import InfoCircle from '~icons/tabler/info-circle';
	import type { AlarmRule } from '$lib/types/domain/alarms';
	import { ALARM_SOURCES, MAX_ALARM_RULES, SEVERITY_CONFIG } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import {
		getAlarmSourceLabel,
		getBooleanAlarmConditionLabel
	} from '$lib/features/alarms/alarmLabels';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton, FormToggle } from '$lib/components/shared/forms';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	interface Props {
		rules: AlarmRule[];
		canManage: boolean;
		onEdit: (_rule: AlarmRule) => void;
		onDelete: (_rule: AlarmRule) => void;
		onToggle: (_rule: AlarmRule) => void;
		onAdd: () => void;
		onInfo: () => void;
	}

	let { rules, canManage, onEdit, onDelete, onToggle, onAdd, onInfo }: Props = $props();
	let maxRulesReached = $derived(rules.length >= MAX_ALARM_RULES);
	let rulesLimitLabel = $derived.by(() =>
		maxRulesReached
			? m.alarms_max_rules_reached({ max: MAX_ALARM_RULES }, { locale: i18n.languageTag })
			: m.alarms_rules_count_status(
					{ current: rules.length, max: MAX_ALARM_RULES },
					{ locale: i18n.languageTag }
				)
	);

	function getSeverityLabel(severity: AlarmRule['severity']) {
		switch (severity) {
			case 'info':
				return m.alarms_severity_info({ locale: i18n.languageTag });
			case 'warning':
				return m.alarms_severity_warning({ locale: i18n.languageTag });
			case 'critical':
				return m.alarms_severity_critical({ locale: i18n.languageTag });
		}
	}
</script>

<BaseCard title={m.alarms_title({ locale: i18n.languageTag })} icon={Bell}>
	{#snippet actions()}
		<div class="flex flex-col items-end gap-1">
			<div class="flex gap-2">
				<FormButton
					label=""
					icon={InfoCircle}
					onclick={onInfo}
					class="btn-square btn-neutral"
					aria-label={m.alarms_info_btn_title(
						{ max: MAX_ALARM_RULES },
						{ locale: i18n.languageTag }
					)}
					title={m.alarms_info_btn_title({ max: MAX_ALARM_RULES }, { locale: i18n.languageTag })}
				/>
				<FormButton
					label={m.alarms_add_btn_with_count(
						{ current: rules.length, max: MAX_ALARM_RULES },
						{ locale: i18n.languageTag }
					)}
					icon={Plus}
					onclick={onAdd}
					disabled={!canManage || maxRulesReached}
					title={rulesLimitLabel}
				/>
			</div>
			<p class:text-warning={maxRulesReached} class="text-[11px] text-right opacity-70">
				{rulesLimitLabel}
			</p>
		</div>
	{/snippet}

	{#if rules.length === 0}
		<div class="text-center py-8 opacity-70">
			<Bell class="w-12 h-12 mx-auto mb-2 opacity-50" />
			<p>{m.alarms_no_rules({ locale: i18n.languageTag })}</p>
			<p class="text-sm">
				{m.alarms_no_rules_hint({ max: MAX_ALARM_RULES }, { locale: i18n.languageTag })}
			</p>
		</div>
	{:else}
		<div class="flex flex-col gap-1">
			{#each rules as rule (rule.id)}
				{@const sourceInfo = ALARM_SOURCES[rule.source] || { unit: '' }}
				{@const severityInfo = SEVERITY_CONFIG[rule.severity]}
				<StatusRow
					class={!rule.enabled ? 'opacity-50' : ''}
					label={rule.name}
					labelClass="font-bold truncate"
				>
					{#snippet iconSlot()}
						<div class="flex h-10 w-10 flex-col items-center justify-center gap-0.5">
							{#if rule.severity === 'critical'}
								<AlertTriangle class="w-5 h-5 text-error" />
							{:else if rule.severity === 'warning'}
								<AlertTriangle class="w-5 h-5 text-warning" />
							{:else}
								<InfoCircle class="w-5 h-5 text-info" />
							{/if}
							<span
								class="text-[10px] uppercase font-bold {rule.enabled
									? 'text-success'
									: 'opacity-40'}"
							>
								{rule.enabled ? 'ON' : 'OFF'}
							</span>
						</div>
					{/snippet}
					{#snippet details()}
						<div class="text-xs opacity-75 flex items-center gap-2 flex-wrap">
							<span
								class="badge {severityInfo.badgeClass} badge-xs uppercase font-bold tracking-wider"
							>
								{getSeverityLabel(rule.severity)}
							</span>
							<span class="opacity-50">•</span>
							{#if sourceInfo.booleanLike}
								<span>{getBooleanAlarmConditionLabel(rule.source)}</span>
								{#if rule.source === 'gpio_digital' && rule.gpio_id}
									<span class="badge badge-ghost badge-xs">{rule.gpio_id}</span>
								{/if}
							{:else}
								<span
									>{getAlarmSourceLabel(rule.source)}
									{rule.operator === 'above' ? '>' : '<'}
									{rule.threshold}{sourceInfo.unit}</span
								>
							{/if}
							<span class="opacity-50 hidden sm:inline">•</span>
							<span class="hidden sm:inline">{rule.notify_channels.join(', ')}</span>
						</div>
					{/snippet}
					{#snippet actions()}
						<FormToggle
							checked={rule.enabled}
							onchange={() => onToggle(rule)}
							disabled={!canManage}
							plain={true}
						/>
						<div class="divider divider-horizontal mx-0 h-6"></div>
						<FormButton
							label=""
							icon={Edit}
							class="btn-ghost btn-sm btn-square"
							onclick={() => onEdit(rule)}
							disabled={!canManage}
							aria-label={m.aria_edit_rule({ locale: i18n.languageTag })}
							title={m.aria_edit_rule({ locale: i18n.languageTag })}
						/>
						<FormButton
							label=""
							icon={Trash}
							class="btn-ghost btn-sm btn-square text-error"
							onclick={() => onDelete(rule)}
							disabled={!canManage}
							aria-label={m.aria_delete_rule({ locale: i18n.languageTag })}
							title={m.aria_delete_rule({ locale: i18n.languageTag })}
						/>
					{/snippet}
				</StatusRow>
			{/each}
		</div>
	{/if}
</BaseCard>
