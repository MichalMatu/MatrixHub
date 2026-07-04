<!--
	MemoryCard.svelte - Memory/Heap status display component
	Shows heap usage, fragmentation, and largest block
-->
<script lang="ts">
	import { formatBytes } from '$lib/utils';
	import type { SystemInformation } from '$lib/types/system/system';
	import type { ExtendedHealthDiagnostics } from '$lib/types/system/systemStatusSnapshot';

	import Heap from '~icons/tabler/box-model';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	interface Props {
		systemInfo: SystemInformation;
		health: ExtendedHealthDiagnostics | null;
	}

	let { systemInfo, health }: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';
</script>

<StatusRow
	icon={Heap}
	iconClass={statIconClass}
	label={m.status_mem_title({ locale: i18n.languageTag })}
>
	{#snippet details()}
		<div class="text-sm opacity-75">
			{m.status_mem_usage(
				{
					free: formatBytes(systemInfo.free_heap),
					total: formatBytes(systemInfo.total_heap),
					frag: (health ? health.heap.fragmentation : 0).toString()
				},
				{ locale: i18n.languageTag }
			)}
			{#if health}
				• {m.status_mem_largest({ locale: i18n.languageTag })}: {formatBytes(health.heap.largest)}
			{/if}
		</div>
	{/snippet}
</StatusRow>
