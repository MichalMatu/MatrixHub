<script lang="ts">
	import Activity from '~icons/tabler/activity';
	import Bluetooth from '~icons/tabler/bluetooth';
	import Search from '~icons/tabler/search';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	interface Props {
		isRunning: boolean;
		isScannerActive: boolean;
	}

	let { isRunning, isScannerActive }: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';
</script>

<BaseCard title={m.ble_status_title({ locale: i18n.languageTag })} icon={Activity} class="h-full">
	<div class="flex w-full flex-col gap-1">
		<StatusRow
			icon={Bluetooth}
			iconClass={statIconClass}
			label={m.ble_service_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div class="text-xs opacity-70 mt-0.5">
					{#if isRunning}
						<span class="badge badge-success badge-sm"
							>{m.ble_status_running({ locale: i18n.languageTag })}</span
						>
					{:else}
						<span class="badge badge-ghost badge-sm"
							>{m.ble_status_stopped({ locale: i18n.languageTag })}</span
						>
					{/if}
				</div>
			{/snippet}
		</StatusRow>

		<StatusRow
			icon={Search}
			iconClass={statIconClass}
			label={m.ble_scanner_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div class="text-xs opacity-70 mt-0.5">
					{#if isRunning && isScannerActive}
						<span class="badge badge-success badge-sm"
							>{m.ble_scanner_on({ locale: i18n.languageTag })}</span
						>
					{:else}
						<span class="badge badge-ghost badge-sm"
							>{m.ble_scanner_off({ locale: i18n.languageTag })}</span
						>
					{/if}
				</div>
			{/snippet}
		</StatusRow>
	</div>
</BaseCard>
