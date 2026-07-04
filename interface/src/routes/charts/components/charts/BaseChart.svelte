<script lang="ts">
	import { onMount, type Component } from 'svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ChartCard from '../ChartCard.svelte';
	import { calcStats } from '$lib/utils/charts';
	import { useUplotChart, loadUplotLib } from '$lib/utils/charts/useUplotChart.svelte';
	import { Spinner } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';
	import type uPlotNamespace from 'uplot';

	/**
	 * Props for BaseChart component.
	 */
	interface Props {
		/** Data series for the chart */
		data: (number | null)[];
		/** Timestamp series corresponding to the data */
		timestamps: number[];
		/** Title of the chart */
		title: string;
		/** Icon component to display in the header */
		icon: Component;
		/** Unique chart identifier */
		chartId: string;
		/** Primary color for the chart line and elements */
		color: string;
		/** Unit of measurement (e.g. "°C", "%") */
		unit: string;
		/** Number of decimals to display in tooltips and stats. Default 1 */
		decimals?: number;
		/** Shadow color variant. Default 'primary' */
		shadowColor?: 'warning' | 'primary' | 'error' | 'success' | 'info';
		/** Fixed Y-axis range [min, max] or specific range function */
		yRange?:
			| [number, number]
			| ((_u: uPlotNamespace, _min: number, _max: number) => [number, number]);
		/** Minimum Y-axis padding in data units. Default 1 */
		minPadding?: number;
		/** Minimum Y-axis value (floor). Auto-range won't go below this. */
		yFloor?: number;
	}

	let {
		data,
		timestamps,
		title,
		icon,
		chartId,
		color,
		unit,
		decimals = 1,
		shadowColor = 'primary',
		yRange,
		minPadding = 1,
		yFloor
	}: Props = $props();

	let chartViewport: HTMLElement;
	let chartMount: HTMLElement;
	let isLoading = $state(true);

	const uplotChart = useUplotChart(() => ({
		chartId,
		title,
		color,
		unit,
		decimals,
		yRange,
		minPadding,
		yFloor
	}));

	const { state: chartState } = uplotChart;
	let stats = $derived(calcStats(data));

	onMount(async () => {
		try {
			await loadUplotLib();
		} finally {
			isLoading = false;
		}
	});

	$effect(() => {
		if (chartViewport && chartMount && !isLoading) {
			uplotChart.mount(chartViewport, chartMount);
		}
	});

	$effect(() => {
		if (data.length > 0 && timestamps.length > 0 && !isLoading) {
			uplotChart.renderChart(data, timestamps);
		}
	});
</script>

<ChartCard
	{title}
	{icon}
	{chartId}
	{shadowColor}
	{stats}
	{unit}
	{decimals}
	selectedPoint={chartState.selectedPoint}
	{color}
>
	<div
		bind:this={chartViewport}
		class="relative w-full overscroll-x-contain snap-x snap-mandatory uplot-scroll"
		class:overflow-x-auto={chartState.shouldScroll}
		class:overflow-x-hidden={!chartState.shouldScroll}
		class:uplot-scroll--active={chartState.shouldScroll}
	>
		{#if isLoading}
			<div
				class="absolute inset-0 flex items-center justify-center bg-base-100/30 z-20 backdrop-blur-[1px]"
			>
				<Spinner />
			</div>
		{/if}
		<div bind:this={chartMount} class="snap-start"></div>
		{#if chartState.isZoomed}
			<FormButton
				label="↺ Reset"
				onclick={() => uplotChart.resetZoom()}
				type="button"
				class="absolute top-2 right-2 btn-xs btn-ghost bg-base-300/80 hover:bg-base-300 z-10"
				title={m.chart_reset_zoom({ locale: i18n.languageTag })}
			/>
		{/if}
	</div>
</ChartCard>
