<script lang="ts">
	import Bell from '~icons/tabler/bell';
	import BellRinging from '~icons/tabler/bell-ringing';
	import BellOff from '~icons/tabler/bell-off';
	import AlertCircle from '~icons/tabler/alert-circle';
	import CheckCircle from '~icons/tabler/check';
	import { ALARM_SOURCES } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { getBooleanAlarmConditionLabel } from '$lib/features/alarms/alarmLabels';
	import BaseWidget from '$lib/components/layout/BaseWidget.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { useAlarmsWidgetManagement } from './useAlarmsWidgetManagement.svelte';

	const controller = useAlarmsWidgetManagement();
</script>

<BaseWidget
	href="/alarms"
	icon={controller.hasActiveAlarms ? BellRinging : Bell}
	title={m.widget_alarms_title({ locale: i18n.languageTag })}
	shadowColor={controller.hasActiveAlarms ? 'error' : 'success'}
	loading={controller.loading}
>
	{#snippet children()}
		{#if controller.errorMessage}
			<div class="alert alert-warning text-xs mb-2">
				<span
					>{m.error_prefix({ error: controller.errorMessage }, { locale: i18n.languageTag })}</span
				>
			</div>
		{/if}
		{#if !controller.hasRules}
			<div class="flex flex-col items-center justify-center h-full text-base-content/50">
				<BellOff class="w-8 h-8 mb-2 opacity-50" />
				<p class="text-sm">{m.widget_alarms_no_alarms({ locale: i18n.languageTag })}</p>
			</div>
		{:else}
			<div class="flex flex-col gap-2">
				{#each controller.rules as rule (rule.id)}
					{@const isTriggered = rule.triggered && rule.enabled}
					<ContentBox
						paddingClass="p-2"
						class="flex items-center gap-3 shadow-sm border-l-4 transition-all {isTriggered
							? 'border-error'
							: !rule.enabled
								? 'border-base-300 opacity-60'
								: 'border-success'}"
					>
						<!-- Icon status -->
						<div class="shrink-0">
							{#if isTriggered}
								<AlertCircle class="w-5 h-5 text-error animate-pulse" />
							{:else if rule.enabled}
								<CheckCircle class="w-5 h-5 text-success" />
							{:else}
								<BellOff class="w-5 h-5 text-base-content/30" />
							{/if}
						</div>

						<!-- Info -->
						<div class="flex-1 min-w-0">
							<div class="font-bold text-sm flex items-center gap-2">
								<span class="truncate">
									{rule.name}
								</span>
							</div>
							<div class="text-[10px] opacity-70 flex items-center gap-1 min-w-0">
								{#if ALARM_SOURCES[rule.source]?.booleanLike}
									<span class="truncate">{getBooleanAlarmConditionLabel(rule.source)}</span>
								{:else}
									<span class="truncate">
										{controller.getSourceLabel(rule.source)}
									</span>
									{#if rule.current_value !== undefined}
										<span class="font-mono font-bold shrink-0"
											>{rule.current_value.toFixed(rule.source === 'co2' ? 0 : 1)}</span
										>
									{/if}
									<span class="shrink-0">{rule.operator === 'above' ? '>' : '<'}</span>
									<span class="shrink-0">{rule.threshold}</span>
									<span class="shrink-0">{ALARM_SOURCES[rule.source]?.unit}</span>
								{/if}
							</div>
						</div>

						<!-- Toggle -->
						<div class="shrink-0">
							<input
								type="checkbox"
								class="toggle toggle-sm toggle-primary"
								checked={rule.enabled}
								disabled={!controller.canManage}
								onclick={(e) => controller.toggleRule(rule, e)}
							/>
						</div>
					</ContentBox>
				{/each}
			</div>
		{/if}
	{/snippet}
</BaseWidget>
