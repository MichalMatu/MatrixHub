<script lang="ts">
	import type { AlarmSource } from '$lib/types/domain/alarms';
	import { ALARM_SOURCES } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { getBooleanAlarmConditionLabel } from '$lib/features/alarms/alarmLabels';

	let {
		threshold = $bindable(),
		source,
		min,
		max,
		step,
		onchange
	}: {
		threshold: number;
		source: AlarmSource;
		min: number;
		max: number;
		step: number;
		onchange?: (value: number) => void;
	} = $props();

	function handleChange(e: Event) {
		const value = Number((e.target as HTMLInputElement).value);
		threshold = value;
		onchange?.(value);
	}
</script>

<div class="flex flex-col gap-1">
	<span class="text-xs font-bold block opacity-70 uppercase tracking-wide">
		{m.alarms_field_threshold({ locale: i18n.languageTag })}
	</span>
	{#if ALARM_SOURCES[source].booleanLike}
		<div class="rounded-md border border-base-300/60 bg-base-100/40 px-3 py-2 text-sm font-medium">
			{getBooleanAlarmConditionLabel(source)}
		</div>
	{:else}
		<div class="flex items-center gap-2">
			<input
				id="alarm-threshold"
				type="range"
				class="range range-primary range-sm flex-1 w-full"
				value={threshold}
				oninput={handleChange}
				{min}
				{max}
				{step}
			/>
			<span class="text-sm w-16 text-right font-mono">
				{threshold}{ALARM_SOURCES[source].unit}
			</span>
		</div>
		<div class="flex justify-between text-[10px] opacity-50 px-1">
			<span>{min}{ALARM_SOURCES[source].unit}</span>
			<span>{max}{ALARM_SOURCES[source].unit}</span>
		</div>
	{/if}
</div>
