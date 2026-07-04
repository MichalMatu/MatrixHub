<script lang="ts">
	import ChartBar from '~icons/tabler/chart-bar';
	import Clock from '~icons/tabler/clock';
	import ChartLine from '~icons/tabler/chart-line';
	import FileText from '~icons/tabler/file-text';
	import Database from '~icons/tabler/database';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';
</script>

<div class="grid grid-cols-1 md:grid-cols-2 gap-4">
	<BaseCard title={m.logs_title({ locale: i18n.languageTag })} icon={FileText}>
		<div class="alert alert-info">
			<span>{m.logs_empty_msg({ locale: i18n.languageTag })}</span>
		</div>
	</BaseCard>

	<BaseCard title={m.logs_info_title({ locale: i18n.languageTag })} icon={ChartBar}>
		<div class="flex flex-col gap-1">
			<StatusRow
				icon={Clock}
				iconClass="h-6 w-6 flex-none text-primary"
				label={m.logs_info_interval({ locale: i18n.languageTag })}
				value="20 min"
			/>

			<StatusRow
				icon={ChartLine}
				iconClass="h-6 w-6 flex-none text-secondary"
				label={m.logs_info_points({ locale: i18n.languageTag })}
				value="~72"
			/>

			<StatusRow
				icon={FileText}
				iconClass="h-6 w-6 flex-none text-accent"
				label={m.logs_info_format({ locale: i18n.languageTag })}
				value={m.logs_binary_format()}
			/>

			<StatusRow
				icon={Database}
				iconClass="h-6 w-6 flex-none text-info"
				label={m.logs_info_storage({ locale: i18n.languageTag })}
				value={m.logs_info_storage_val({ locale: i18n.languageTag })}
			/>
		</div>
		<div class="mt-4 pt-1">
			<div class="alert alert-info text-sm py-3 opacity-90">
				<span>{m.logs_desc({ locale: i18n.languageTag })}</span>
			</div>
		</div>
	</BaseCard>
</div>
