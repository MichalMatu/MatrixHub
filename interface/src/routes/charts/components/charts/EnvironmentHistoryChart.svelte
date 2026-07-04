<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import type uPlotNamespace from 'uplot';
	import type { AlignedData, Options } from 'uplot';
	import ChartLine from '~icons/tabler/chart-line';
	import Refresh from '~icons/tabler/refresh';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { Spinner } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { CHART_CONFIGS } from '../../chartConfigs';
	import { loadUplotLib } from '$lib/utils/charts/useUplotChart.svelte';
	import { wheelZoomPlugin } from '$lib/utils/charts/wheelZoomPlugin';
	import { formatTimeOnlyTick } from '$lib/utils/charts/timeFormatters';
	import {
		buildEnvironmentPlotData,
		buildEnvironmentSeriesModels,
		formatSensorValue,
		getEnvironmentSampleValues,
		type EnvironmentSensorKey,
		type EnvironmentPlotData,
		type EnvironmentSeriesModel
	} from './environmentHistoryModel';

	interface Props {
		timestamps: number[];
		co2: (number | null)[];
		temperature: (number | null)[];
		humidity: (number | null)[];
	}

	let { timestamps, co2, temperature, humidity }: Props = $props();

	let chartViewport: HTMLElement;
	let chartMount: HTMLElement;
	let chart: uPlotNamespace | undefined;
	let uPlotLib = $state<typeof uPlotNamespace | null>(null);
	let resizeObserver: ResizeObserver | undefined;
	let resizeTimer: ReturnType<typeof setTimeout> | undefined;
	let isLibraryLoading = $state(true);
	let isZoomed = $state(false);
	let selectedIndex = $state<number | null>(null);
	let tooltipLeft = $state(160);
	let visibleKeys = $state<EnvironmentSensorKey[]>(['co2', 'temperature', 'humidity']);
	let initialXRange: { min: number; max: number } | null = null;

	const TRACK_GAP_SECONDS = 60 * 60;
	const SENSOR_BANDS = {
		co2: { min: 70, max: 94 },
		temperature: { min: 38, max: 62 },
		humidity: { min: 6, max: 30 }
	} as const;

	const locale = $derived(i18n.languageTag);
	const seriesModels = $derived(
		buildEnvironmentSeriesModels([
			{
				key: 'co2',
				label: 'CO₂',
				color: CHART_CONFIGS.co2.color,
				unit: CHART_CONFIGS.co2.unit,
				decimals: CHART_CONFIGS.co2.decimals,
				data: co2,
				minPlotRange: 200,
				band: SENSOR_BANDS.co2
			},
			{
				key: 'temperature',
				label: m.dashboard_temp({ locale }),
				color: CHART_CONFIGS.temperature.color,
				unit: CHART_CONFIGS.temperature.unit,
				decimals: CHART_CONFIGS.temperature.decimals,
				data: temperature,
				minPlotRange: 5,
				band: SENSOR_BANDS.temperature
			},
			{
				key: 'humidity',
				label: m.dashboard_humid({ locale }),
				color: CHART_CONFIGS.humidity.color,
				unit: CHART_CONFIGS.humidity.unit,
				decimals: CHART_CONFIGS.humidity.decimals,
				data: humidity,
				minPlotRange: 10,
				band: SENSOR_BANDS.humidity
			}
		])
	);
	const environmentPlotData = $derived(
		buildEnvironmentPlotData(timestamps, seriesModels, TRACK_GAP_SECONDS)
	);

	const selectedTimestamp = $derived(
		selectedIndex == null ? null : (timestamps[selectedIndex] ?? null)
	);
	const selectedValues = $derived(getEnvironmentSampleValues(seriesModels, selectedIndex));
	const activeMetricLabel = $derived(
		selectedIndex == null
			? m.charts_metric_latest({ locale })
			: m.charts_metric_selected({ locale })
	);
	const sampleCountText = $derived(
		m.charts_environment_samples({ count: timestamps.length }, { locale })
	);

	onMount(async () => {
		setupResizeObserver();
		try {
			uPlotLib = await loadUplotLib();
		} finally {
			isLibraryLoading = false;
		}
	});

	onDestroy(() => {
		clearTimeout(resizeTimer);
		resizeObserver?.disconnect();
		destroyChart();
	});

	$effect(() => {
		const models = seriesModels;
		const plotData = environmentPlotData;

		if (chartViewport && chartMount && uPlotLib && plotData.timestamps.length > 0) {
			renderChart(models, plotData);
		}
	});

	$effect(() => {
		const keys = visibleKeys;
		const models = seriesModels;
		applySeriesVisibility(models, keys);
	});

	function setupResizeObserver() {
		if (!chartViewport || typeof ResizeObserver === 'undefined') return;

		resizeObserver = new ResizeObserver(() => {
			clearTimeout(resizeTimer);
			resizeTimer = setTimeout(() => {
				if (!chart) return;
				chart.setSize(getChartSize());
			}, 120);
		});
		resizeObserver.observe(chartViewport);
	}

	function getChartSize() {
		const width = Math.max(280, Math.floor((chartViewport?.clientWidth ?? 320) - 2));
		const height = width < 420 ? 310 : width < 768 ? 360 : 420;
		return { width, height };
	}

	function renderChart(models: EnvironmentSeriesModel[], plotData: EnvironmentPlotData) {
		if (!uPlotLib || !chartMount) return;

		destroyChart();
		selectedIndex = null;
		isZoomed = false;

		const { width, height } = getChartSize();
		chartMount.style.width = `${width}px`;
		const linearPath = uPlotLib.paths.linear?.();
		if (!linearPath) {
			throw new Error('uPlot linear path helper is unavailable.');
		}

		const opts: Options = {
			width,
			height,
			padding: width < 420 ? [12, 10, 0, 0] : [14, 14, 0, 0],
			scales: {
				x: { time: true },
				y: { range: [0, 100] }
			},
			series: [
				{ label: 'Time' },
				...models.map((model) => ({
					label: model.label,
					scale: 'y',
					stroke: model.color,
					width: 2,
					paths: linearPath,
					spanGaps: false,
					points: {
						show: true,
						size: width < 420 ? 3 : 4,
						width: 0,
						fill: model.color,
						stroke: model.color
					}
				}))
			],
			axes: [
				{
					scale: 'x',
					stroke: '#9ca3af',
					grid: { stroke: 'rgba(148, 163, 184, 0.16)', width: 1 },
					ticks: { stroke: 'rgba(148, 163, 184, 0.28)', width: 1, size: 4 },
					gap: 4,
					space: width < 420 ? 80 : 60,
					size: width < 420 ? 32 : 38,
					values: (u, vals: number[]) =>
						vals.map((v, i, arr) => formatTimeOnlyTick(v, i, arr, u.scales.x))
				},
				{
					scale: 'y',
					show: false
				}
			],
			cursor: {
				drag: { x: true, y: false, setScale: true },
				focus: { prox: 24 },
				points: { size: 7, width: 2, fill: '#111827', stroke: '#f8fafc' }
			},
			plugins: [wheelZoomPlugin()],
			legend: { show: false }
		};

		const alignedData = [
			plotData.timestamps,
			...models.map((model) => plotData.series[model.key])
		] as AlignedData;
		chart = new uPlotLib(opts, alignedData, chartMount);
		initialXRange = {
			min: plotData.timestamps[0],
			max: plotData.timestamps[plotData.timestamps.length - 1]
		};

		setupCursorHandlers(plotData.rawIndexes);
		setupZoomTracking();
		applySeriesVisibility(models, visibleKeys);
	}

	function destroyChart() {
		chart?.destroy();
		chart = undefined;
	}

	function setupCursorHandlers(rawIndexes: (number | null)[]) {
		if (!chart) return;

		chart.over.addEventListener('mousemove', () => {
			if (!chart) return;
			const index = chart.cursor.idx;
			const left = chart.cursor.left ?? 0;
			const viewportWidth = chartViewport?.clientWidth ?? 320;
			tooltipLeft = Math.min(Math.max(left, 112), Math.max(112, viewportWidth - 112));
			selectedIndex = typeof index === 'number' && index >= 0 ? (rawIndexes[index] ?? null) : null;
		});

		chart.over.addEventListener('mouseleave', () => {
			selectedIndex = null;
		});

		chart.over.addEventListener('dblclick', () => {
			resetZoom();
		});
	}

	function setupZoomTracking() {
		if (!chart) return;

		if (!chart.hooks.setScale) {
			chart.hooks.setScale = [];
		}

		chart.hooks.setScale.push((u, scaleKey) => {
			if (scaleKey !== 'x' || !initialXRange) return;
			const initialRange = initialXRange.max - initialXRange.min;
			if (initialRange <= 0) {
				isZoomed = false;
				return;
			}

			const currentMin = u.scales.x.min ?? initialXRange.min;
			const currentMax = u.scales.x.max ?? initialXRange.max;
			const currentRange = currentMax - currentMin;
			isZoomed = Math.abs(currentRange - initialRange) / initialRange >= 0.01;
		});
	}

	function resetZoom() {
		if (!chart || !initialXRange) return;

		const range = initialXRange;
		chart.batch(() => {
			chart?.setScale('x', { min: range.min, max: range.max });
		});
		isZoomed = false;
	}

	function isSeriesVisible(key: EnvironmentSensorKey) {
		return visibleKeys.includes(key);
	}

	function toggleSeries(key: EnvironmentSensorKey) {
		const currentlyVisible = isSeriesVisible(key);
		if (currentlyVisible && visibleKeys.length === 1) return;

		visibleKeys = currentlyVisible
			? visibleKeys.filter((visibleKey) => visibleKey !== key)
			: [...visibleKeys, key];
	}

	function applySeriesVisibility(models: EnvironmentSeriesModel[], keys: EnvironmentSensorKey[]) {
		if (!chart) return;

		models.forEach((model, index) => {
			chart?.setSeries(index + 1, {
				show: keys.includes(model.key)
			});
		});
	}

	function getDisplayValue(series: EnvironmentSeriesModel) {
		if (selectedIndex != null) return series.data[selectedIndex] ?? null;
		return series.stats?.latest ?? null;
	}

	function formatTimestamp(timestamp: number) {
		const ms = timestamp < 100_000_000_000 ? timestamp * 1000 : timestamp;
		return new Intl.DateTimeFormat(locale, {
			hour: '2-digit',
			minute: '2-digit'
		}).format(new Date(ms));
	}

	function bandTopPercent(series: EnvironmentSeriesModel) {
		return 100 - series.band.max + 1;
	}

	function formatSeriesRange(series: EnvironmentSeriesModel) {
		if (!series.stats) return '';
		return `${formatSensorValue(series.stats.min, series.decimals, series.unit)} - ${formatSensorValue(
			series.stats.max,
			series.decimals,
			series.unit
		)}`;
	}
</script>

<BaseCard
	title={m.charts_environment_title({ locale })}
	icon={ChartLine}
	class="environment-history-card"
	bleed={true}
	hideTitleOnTiny={false}
>
	{#snippet actions()}
		<div class="flex items-center gap-2">
			<span class="hidden text-xs text-base-content/60 sm:inline">
				{selectedTimestamp == null ? sampleCountText : formatTimestamp(selectedTimestamp)}
			</span>
			{#if isZoomed}
				<FormButton
					variant="icon"
					size="xs"
					icon={Refresh}
					onclick={resetZoom}
					title={m.chart_reset_zoom({ locale })}
					ariaLabel={m.chart_reset_zoom({ locale })}
				/>
			{/if}
		</div>
	{/snippet}

	<div class="space-y-3 px-1">
		<div class="grid gap-2 sm:grid-cols-3">
			{#each seriesModels as series (series.key)}
				<button
					type="button"
					class="min-w-0 rounded-md border border-base-content/10 bg-base-100/50 px-3 py-2 text-left transition hover:border-base-content/20 hover:bg-base-100/70"
					class:opacity-50={!isSeriesVisible(series.key)}
					aria-pressed={isSeriesVisible(series.key)}
					aria-label={m.charts_series_toggle({ name: series.label }, { locale })}
					onclick={() => toggleSeries(series.key)}
				>
					<div class="mb-1 flex items-center justify-between gap-2">
						<span
							class="flex min-w-0 items-center gap-2 text-xs font-semibold text-base-content/70"
						>
							<span
								class="h-2.5 w-2.5 shrink-0 rounded-full"
								style="background-color: {series.color}; box-shadow: 0 0 0 1px {series.color}55;"
							></span>
							<span class="truncate">{series.label}</span>
						</span>
						<span class="text-[10px] uppercase tracking-normal text-base-content/45">
							{activeMetricLabel}
						</span>
					</div>
					<div class="font-mono text-lg font-semibold leading-tight">
						{formatSensorValue(getDisplayValue(series), series.decimals, series.unit)}
					</div>
					{#if series.stats}
						<div class="mt-1 truncate text-xs text-base-content/55">
							{m.chart_stat_avg({ locale })}: {formatSensorValue(
								series.stats.avg,
								series.decimals,
								series.unit
							)}
							<span class="mx-1 text-base-content/25">|</span>
							{m.charts_metric_range({ locale })}: {formatSensorValue(
								series.stats.min,
								series.decimals,
								series.unit
							)}
							-
							{formatSensorValue(series.stats.max, series.decimals, series.unit)}
						</div>
					{:else}
						<div class="mt-1 text-xs text-base-content/55">
							{m.charts_no_sensor_value({ locale })}
						</div>
					{/if}
				</button>
			{/each}
		</div>

		<div
			bind:this={chartViewport}
			class="relative min-h-[310px] overflow-hidden rounded-md border border-base-content/10 bg-base-100/40 sm:min-h-[360px] lg:min-h-[420px]"
		>
			{#if isLibraryLoading}
				<div class="absolute inset-0 z-20 flex items-center justify-center bg-base-100/40">
					<Spinner />
				</div>
			{/if}
			<div bind:this={chartMount}></div>

			<div class="pointer-events-none absolute inset-0">
				<div class="absolute inset-x-3 top-1/3 h-px bg-base-content/10"></div>
				<div class="absolute inset-x-3 top-2/3 h-px bg-base-content/10"></div>
				{#each seriesModels as series (series.key)}
					<div
						class="absolute left-3 right-3 flex items-center justify-between gap-2 text-[10px] uppercase tracking-normal text-base-content/50"
						class:opacity-40={!isSeriesVisible(series.key)}
						style="top: {bandTopPercent(series)}%;"
					>
						<span class="flex min-w-0 items-center gap-1.5">
							<span
								class="h-1.5 w-1.5 shrink-0 rounded-full"
								style="background-color: {series.color};"
							></span>
							<span class="truncate">{series.label}</span>
						</span>
						<span class="hidden shrink-0 font-mono text-base-content/45 sm:inline">
							{formatSeriesRange(series)}
						</span>
					</div>
				{/each}
			</div>

			{#if selectedTimestamp != null}
				<div
					class="pointer-events-none absolute top-3 z-10 w-56 -translate-x-1/2 rounded-md border border-base-content/10 bg-base-200/95 p-2 text-xs shadow-lg backdrop-blur"
					style="left: {tooltipLeft}px;"
				>
					<div class="mb-1 font-mono text-[11px] text-base-content/55">
						{formatTimestamp(selectedTimestamp)}
					</div>
					<div class="space-y-1">
						{#each selectedValues as sample (sample.key)}
							<div class="flex items-center justify-between gap-2">
								<span class="flex min-w-0 items-center gap-2 text-base-content/70">
									<span
										class="h-2 w-2 shrink-0 rounded-full"
										style="background-color: {sample.color};"
									></span>
									<span class="truncate">{sample.label}</span>
								</span>
								<span class="font-mono font-semibold">
									{formatSensorValue(sample.value, sample.decimals, sample.unit)}
								</span>
							</div>
						{/each}
					</div>
				</div>
			{/if}

			<div
				class="pointer-events-none absolute right-2 top-2 rounded bg-base-200/75 px-2 py-1 text-[10px] uppercase tracking-normal text-base-content/45"
			>
				{m.charts_environment_trend_scale({ locale })}
			</div>
		</div>
	</div>
</BaseCard>

<style>
	:global(.environment-history-card .u-wrap) {
		width: 100%;
	}

	:global(.environment-history-card .u-over) {
		cursor: crosshair;
	}

	:global(.environment-history-card .u-axis text) {
		fill: color-mix(in srgb, currentColor, transparent 22%);
		font-size: 11px;
	}

	@media (max-width: 420px) {
		:global(.environment-history-card .u-axis text) {
			font-size: 10px;
		}
	}
</style>
