<script lang="ts">
	import { onMount } from 'svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import type { CsiAlarmBand } from '$lib/types/connectivity/wifiSensing';
	import * as m from '$lib/paraglide/messages.js';
	import { formatCsiTimestamp } from './csiWaterfallRaster';
	import { useCsiWaterfallChart } from './useCsiWaterfallChart.svelte';

	interface Props {
		amplitudes: Float32Array;
		subcarriers: number;
		timestamp: number;
		fps: number;
		bands?: CsiAlarmBand[];
	}

	let { amplitudes, subcarriers, timestamp, fps, bands = [] }: Props = $props();

	let canvasWater: HTMLCanvasElement;
	const chart = useCsiWaterfallChart(() => amplitudes, { getCanvas: () => canvasWater });
	const gridLines = $derived.by(() => {
		const ticks: number[] = [];
		const carrierCount = Math.max(subcarriers || amplitudes.length, 1);

		for (let tick = 0; tick < carrierCount; tick += 20) {
			ticks.push(tick);
		}

		return ticks;
	});
	const carrierCount = $derived(Math.max(subcarriers || amplitudes.length, 1));

	function bandStyle(band: CsiAlarmBand) {
		const start = Math.max(0, Math.min(carrierCount - 1, band.start));
		const end = Math.max(0, Math.min(carrierCount - 1, band.end));
		const left = (Math.min(start, end) / carrierCount) * 100;
		const width = ((Math.abs(end - start) + 1) / carrierCount) * 100;
		return `left:${left}%;width:${width}%`;
	}

	$effect(() => {
		if (amplitudes.length > 0 && canvasWater) {
			chart.update(amplitudes);
		}
	});

	onMount(() => {
		chart.init();
		return () => chart.destroy();
	});
</script>

<BaseCard title={m.csi_waterfall_title()}>
	{#snippet actions()}
		<div class="flex items-center gap-2 sm:gap-4 text-xs font-mono opacity-80 mr-2">
			<div class="flex items-center">
				<span class="hidden sm:inline">{m.csi_time_label()}:&nbsp;</span>
				<span class="font-bold inline-block min-w-[12ch]">{formatCsiTimestamp(timestamp)}</span>
			</div>
			<div class="flex items-center">
				<span>{m.csi_fps_label()}:&nbsp;</span>
				<span class="text-info font-bold inline-block min-w-[3ch] text-right">{fps}</span>
			</div>
		</div>
	{/snippet}
	<div class="relative w-full h-[300px] bg-black rounded overflow-hidden">
		<canvas bind:this={canvasWater} class="w-full h-full block"></canvas>

		{#each gridLines as tick}
			<div
				class="absolute top-0 bottom-0 w-[1px] bg-[#333] pointer-events-none"
				style="left: {((tick + 0.5) / Math.max(subcarriers || amplitudes.length, 1)) * 100}%"
			></div>
		{/each}
		{#each bands as band}
			<div
				class="pointer-events-none absolute top-0 bottom-0 border-x border-info/70 bg-info/10"
				style={bandStyle(band)}
			></div>
		{/each}
	</div>
</BaseCard>
