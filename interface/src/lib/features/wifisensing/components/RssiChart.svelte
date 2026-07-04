<script lang="ts">
	import { onMount } from 'svelte';
	import { Spinner } from '$lib/components';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import { useRssiChart, type RssiChartSample } from './useRssiChart.svelte';
	import { loadUplotLib } from '$lib/utils/charts/useUplotChart.svelte';
	import Wifi from '~icons/tabler/wifi';

	interface Props {
		samples: RssiChartSample[];
		threshold: number;
		motionDetected: boolean;
		lastUpdate: number;
		windowMs?: number;
	}

	let { samples, threshold, motionDetected, lastUpdate, windowMs }: Props = $props();

	let chartViewport: HTMLElement;
	let chartMount: HTMLElement;
	let chart: ReturnType<typeof useRssiChart> | undefined;
	let isLoading = $state(true);

	onMount(() => {
		let isDisposed = false;

		void (async () => {
			try {
				const uPlotLib = await loadUplotLib();
				if (!uPlotLib || isDisposed) return;

				chart = useRssiChart(
					() => samples,
					() => motionDetected,
					{ viewport: chartViewport, mount: chartMount },
					uPlotLib
				);
				chart.render();
				chart.setupResizeObserver();
			} finally {
				if (!isDisposed) {
					isLoading = false;
				}
			}
		})();

		return () => {
			isDisposed = true;
			chart?.destroy();
			chart = undefined;
		};
	});

	$effect(() => {
		// Trigger update when dependencies change
		samples.length; // Track length for mutations
		motionDetected;
		if (chart && !isLoading) {
			chart.update();
		}
	});
</script>

<BaseCard
	title={m.sensing_rssi_label({ locale: i18n.languageTag })}
	icon={Wifi}
	iconClass="w-4 h-4 flex-none"
	class="mb-4 transition-all duration-300 {motionDetected
		? 'card-shadow-warning'
		: 'card-shadow-info'}"
>
	{#snippet actions()}
		<div class="flex gap-3 text-xs opacity-70 items-center tabular-nums">
			<span class="min-w-[80px] text-right">
				{m.sensing_buffer_details(
					{
						count: samples.length,
						window: ((windowMs ?? 0) / 1000).toFixed(1)
					},
					{ locale: i18n.languageTag }
				)}
			</span>
			<span class="hidden sm:inline min-w-[120px]"
				>{m.sensing_updated_label({ locale: i18n.languageTag })}:
				{new Date(lastUpdate).toLocaleTimeString()}</span
			>
			<span class="border-l pl-3 border-base-content/20"
				>{m.sensing_threshold_label({ locale: i18n.languageTag })}: {threshold}</span
			>
			{#if motionDetected}
				<span class="text-warning font-bold animate-pulse">⚠ {m.sensing_motion_alert()}</span>
			{/if}
		</div>
	{/snippet}

	<div id="rssiChart" class="relative mt-1">
		{#if isLoading}
			<div
				class="absolute inset-0 flex items-center justify-center bg-base-100/30 z-20 backdrop-blur-[1px]"
			>
				<Spinner />
			</div>
		{/if}
		<div
			bind:this={chartViewport}
			class="relative w-full h-[110px] sm:h-[130px] lg:h-[150px] overflow-hidden"
		>
			<div bind:this={chartMount} class="w-full h-full"></div>
		</div>
	</div>
</BaseCard>
