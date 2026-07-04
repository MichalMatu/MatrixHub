<!--
	StorageCard.svelte - Storage information display (PSRAM, FS, Sketch)
	Shows file system usage, PSRAM, and sketch size
-->
<script lang="ts">
	import { formatBytes } from '$lib/utils';
	import type { SystemInformation } from '$lib/types/system/system';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	import Pyramid from '~icons/tabler/pyramid';
	import Folder from '~icons/tabler/folder';
	import Sketch from '~icons/tabler/chart-pie';
	import Cpu from '~icons/tabler/cpu';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		systemInfo: SystemInformation;
	}

	let { systemInfo }: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';
</script>

<!-- PSRAM -->
{#if systemInfo.psram_size}
	<StatusRow
		icon={Pyramid}
		iconClass={statIconClass}
		label={m.status_psram({ locale: i18n.languageTag })}
		value={m.status_storage_usage(
			{
				percent: ((systemInfo.used_psram / systemInfo.psram_size) * 100).toFixed(1),
				total: formatBytes(systemInfo.psram_size),
				free: formatBytes(systemInfo.free_psram)
			},
			{ locale: i18n.languageTag }
		)}
	/>
{/if}

<!-- File System -->
<StatusRow
	icon={Folder}
	iconClass={statIconClass}
	label={m.status_filesystem({ locale: i18n.languageTag })}
	value={m.status_storage_usage(
		{
			percent: ((systemInfo.fs_used / systemInfo.fs_total) * 100).toFixed(1),
			total: formatBytes(systemInfo.fs_total),
			free: formatBytes(systemInfo.fs_total - systemInfo.fs_used)
		},
		{ locale: i18n.languageTag }
	)}
/>

<!-- LP SRAM (RTC Memory) -->
{#if systemInfo.lp_sram_total}
	<StatusRow
		icon={Cpu}
		iconClass={statIconClass}
		label={m.status_lpsram({ locale: i18n.languageTag })}
		value={m.status_storage_usage(
			{
				percent: ((systemInfo.lp_sram_used! / systemInfo.lp_sram_total) * 100).toFixed(1),
				total: formatBytes(systemInfo.lp_sram_total),
				free: formatBytes(systemInfo.lp_sram_free!)
			},
			{ locale: i18n.languageTag }
		)}
	/>
{/if}

<!-- Sketch -->
<StatusRow
	icon={Sketch}
	iconClass={statIconClass}
	label={m.status_sketch({ locale: i18n.languageTag })}
	value={formatBytes(systemInfo.sketch_size)}
/>
