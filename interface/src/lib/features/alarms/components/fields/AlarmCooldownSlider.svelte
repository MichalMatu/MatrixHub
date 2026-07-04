<script lang="ts">
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import InfoCircle from '~icons/tabler/info-circle';

	let { cooldownSeconds = $bindable() }: { cooldownSeconds: number } = $props();
</script>

<div class="flex flex-col gap-1">
	<span class="text-xs font-bold flex items-center gap-1 opacity-70 uppercase tracking-wide">
		{m.alarms_field_cooldown({ locale: i18n.languageTag })}
		<div
			class="tooltip tooltip-top font-normal"
			data-tip={m.alarms_field_cooldown_desc({ locale: i18n.languageTag })}
		>
			<InfoCircle class="w-3 h-3 hover:opacity-100 cursor-help" />
		</div>
	</span>
	<div class="flex items-center gap-2">
		<input
			id="alarm-cooldown"
			type="range"
			class="range range-primary range-sm flex-1 w-full"
			bind:value={cooldownSeconds}
			min="60"
			max="3600"
			step="60"
		/>
		<span class="text-sm w-16 text-right font-mono">
			{Math.floor(cooldownSeconds / 60)}
			{m.unit_min({ locale: i18n.languageTag })}
		</span>
	</div>
	<div class="flex justify-between text-[10px] opacity-50 px-1">
		<span>1 {m.unit_min({ locale: i18n.languageTag })}</span>
		<span>60 {m.unit_min({ locale: i18n.languageTag })}</span>
	</div>
</div>
