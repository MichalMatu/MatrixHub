<script lang="ts">
	import type { AlarmSeverity } from '$lib/types/domain/alarms';
	import { SEVERITY_CONFIG } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let { severity = $bindable() }: { severity: AlarmSeverity } = $props();

	// Map severity keys to translation functions
	function getSeverityLabel(key: string): string {
		const labels: Record<string, () => string> = {
			info: () => m.alarms_severity_info({ locale: i18n.languageTag }),
			warning: () => m.alarms_severity_warning({ locale: i18n.languageTag }),
			critical: () => m.alarms_severity_critical({ locale: i18n.languageTag })
		};
		return labels[key]?.() ?? key;
	}
</script>

<div class="flex flex-col gap-1">
	<span class="text-xs font-bold block opacity-70 uppercase tracking-wide">
		{m.alarms_field_severity({ locale: i18n.languageTag })}
	</span>
	<div class="flex flex-wrap gap-1">
		{#each Object.entries(SEVERITY_CONFIG) as [key, info]}
			<label
				class="cursor-pointer flex items-center gap-1.5 p-1.5 rounded-lg border border-transparent bg-base-100 hover:border-base-content/20 transition-all"
			>
				<input
					type="radio"
					name="severity"
					class="radio radio-primary radio-xs"
					value={key}
					checked={severity === key}
					onchange={() => (severity = key as AlarmSeverity)}
				/>
				<span class="badge badge-sm {info.badgeClass}">{getSeverityLabel(key)}</span>
			</label>
		{/each}
	</div>
</div>
