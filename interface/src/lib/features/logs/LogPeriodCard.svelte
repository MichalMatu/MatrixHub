<script lang="ts">
	import Clock from '~icons/tabler/clock';
	import { i18n } from '$lib/i18n.svelte';

	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	let {
		oldestMonth,
		newestMonth,
		totalMonths
	}: { oldestMonth: string; newestMonth: string; totalMonths: number } = $props();
</script>

<BaseCard title={m.logs_period_title({ locale: i18n.languageTag })} icon={Clock}>
	<div class="flex flex-col gap-1">
		<StatusRow
			icon={Clock}
			label={m.logs_period_oldest({ locale: i18n.languageTag })}
			value={oldestMonth}
		/>

		<StatusRow
			icon={Clock}
			label={m.logs_period_newest({ locale: i18n.languageTag })}
			value={newestMonth}
		/>

		<StatusRow
			icon={Clock}
			label={m.logs_period_count({ locale: i18n.languageTag })}
			value={totalMonths}
		/>
	</div>
</BaseCard>
