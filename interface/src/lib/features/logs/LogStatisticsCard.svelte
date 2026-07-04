<script lang="ts">
	import ChartBar from '~icons/tabler/chart-bar';
	import FileText from '~icons/tabler/file-text';
	import Database from '~icons/tabler/database';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	let {
		totalFiles,
		totalSizeKB,
		averageSizeKB,
		estimatedEntries
	}: {
		totalFiles: number;
		totalSizeKB: number;
		averageSizeKB: number;
		estimatedEntries: number;
	} = $props();
</script>

<BaseCard title={m.logs_stats_title({ locale: i18n.languageTag })} icon={ChartBar}>
	<div class="flex flex-col gap-1">
		<StatusRow
			icon={FileText}
			iconClass="h-6 w-6 flex-none text-primary"
			label={m.logs_stat_files({ locale: i18n.languageTag })}
			value={totalFiles}
		/>

		<StatusRow
			icon={Database}
			iconClass="h-6 w-6 flex-none text-secondary"
			label={m.logs_stat_size({ locale: i18n.languageTag })}
		>
			{#snippet details()}
				<div class="text-sm opacity-75">
					{totalSizeKB.toFixed(1)} KB
					<span class="text-xs opacity-50 ml-1">({(totalSizeKB / 1024).toFixed(2)} MB)</span>
				</div>
			{/snippet}
		</StatusRow>

		<StatusRow
			icon={ChartBar}
			iconClass="h-6 w-6 flex-none text-accent"
			label={m.logs_stat_avg({ locale: i18n.languageTag })}
			value={`${averageSizeKB.toFixed(1)} KB`}
		/>

		<StatusRow
			icon={FileText}
			iconClass="h-6 w-6 flex-none text-info"
			label={m.logs_stat_entries({ locale: i18n.languageTag })}
			value={estimatedEntries.toLocaleString()}
			subValue={m.logs_stat_entries_desc(
				{ count: Math.floor(estimatedEntries / totalFiles || 0) },
				{ locale: i18n.languageTag }
			)}
		/>
	</div>
</BaseCard>
