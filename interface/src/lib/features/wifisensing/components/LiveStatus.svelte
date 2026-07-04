<script lang="ts">
	import Activity from '~icons/tabler/activity';
	import Wifi from '~icons/tabler/wifi';
	import Signal from '~icons/tabler/antenna-bars-5';
	import ChartIcon from '~icons/tabler/chart-line';
	import Router from '~icons/tabler/router';
	import type { WifiSensingData } from '$lib/types/connectivity/wifiSensing';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	interface Props {
		sensingData: WifiSensingData | null;
		isActive: boolean;
		motionDetected: boolean;
		threshold: number;
	}

	let { sensingData, isActive, motionDetected, threshold }: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';
</script>

<BaseCard title={m.sensing_live_title({ locale: i18n.languageTag })} icon={Activity} class="h-full">
	<div class="flex w-full flex-col gap-1">
		<!-- Status -->
		<StatusRow
			icon={Wifi}
			iconClass={statIconClass}
			label={m.sensing_status_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div class="text-xs opacity-70 mt-0.5">
					{#if isActive}
						<span class="badge badge-success badge-sm"
							>{m.sensing_status_active({ locale: i18n.languageTag })}</span
						>
					{:else}
						<span class="badge badge-ghost badge-sm"
							>{m.sensing_status_inactive({ locale: i18n.languageTag })}</span
						>
					{/if}
				</div>
			{/snippet}
		</StatusRow>

		<!-- Signal & Statistics -->
		<StatusRow
			icon={Signal}
			iconClass={statIconClass}
			label={m.sensing_rssi_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div
					class="flex flex-col sm:flex-row sm:items-baseline gap-x-3 gap-y-0 text-xs opacity-70 mt-0.5"
				>
					<span class="font-bold tabular-nums text-base-content"
						>{sensingData?.stats?.current ?? '-'} dBm</span
					>
					<span class="opacity-80 font-mono truncate">
						{m.sensing_stats_details(
							{
								min: sensingData?.stats?.min ?? '-',
								max: sensingData?.stats?.max ?? '-',
								avg: sensingData?.stats?.avg?.toFixed(1) ?? '-'
							},
							{ locale: i18n.languageTag }
						)}
					</span>
				</div>
			{/snippet}
		</StatusRow>

		<!-- Variance -->
		<StatusRow
			icon={ChartIcon}
			iconClass={statIconClass}
			label={m.sensing_variance_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div class="text-xs opacity-70 mt-0.5 flex items-center gap-2 flex-wrap">
					<span class="font-mono">{sensingData?.stats?.variance?.toFixed(2) ?? '-'}</span>
					{'<'}
					{#if motionDetected}
						<span class="text-warning font-bold">{(threshold * 0.6).toFixed(1)}</span>
						<span class="text-[0.6rem] opacity-60 uppercase ml-0.5">{m.sensing_status_clear()}</span
						>
					{:else}
						<span>{threshold}</span>
						<span class="text-[0.6rem] opacity-60 uppercase ml-0.5">{m.sensing_status_trig()}</span>
					{/if}
					{#if motionDetected}
						<span class="badge badge-warning badge-sm animate-pulse ml-1"
							>{m.sensing_badge_motion({ locale: i18n.languageTag })}</span
						>
					{:else}
						<span class="badge badge-ghost badge-sm ml-1 opacity-50"
							>{m.sensing_badge_no_motion({ locale: i18n.languageTag })}</span
						>
					{/if}
				</div>
			{/snippet}
		</StatusRow>

		<!-- Connection -->
		<StatusRow
			icon={Router}
			iconClass={statIconClass}
			label={m.sensing_connected_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div class="text-xs opacity-70 mt-0.5 font-mono">
					{m.sensing_channel_info(
						{
							ssid: sensingData?.connectedSSID ?? '-',
							channel: sensingData?.connectedChannel ?? '-'
						},
						{ locale: i18n.languageTag }
					)}
				</div>
			{/snippet}
		</StatusRow>

		<!-- Samples -->
		<StatusRow
			icon={ChartIcon}
			iconClass={statIconClass}
			label={m.sensing_buffer_label({ locale: i18n.languageTag })}
			labelClass="font-bold text-sm"
		>
			{#snippet details()}
				<div class="text-xs opacity-70 mt-0.5 tabular-nums">
					{m.sensing_buffer_details(
						{
							count: sensingData?.stats?.sampleCount ?? 0,
							window: ((sensingData?.stats?.windowMs ?? 0) / 1000).toFixed(1)
						},
						{ locale: i18n.languageTag }
					)}
				</div>
			{/snippet}
		</StatusRow>
	</div>
</BaseCard>
