<script lang="ts">
	import type { Component } from 'svelte';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	// Direct import to avoid circular dependency through $lib/components barrel
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import Sparkline from '$lib/components/indicators/Sparkline.svelte';
	import type { Stats } from '$lib/utils/validation/sensorClassification';

	interface Props {
		// Card styling
		href?: string;
		shadowColor: 'success' | 'error' | 'info' | 'warning';
		// Stable ID for sparkline gradient (SSR-safe)
		sparklineId: string;
		// Header
		title: string;
		icon: Component; // Any icon component

		// Sensor data
		value: number | null;
		unit: string;
		hasError?: boolean;

		// Chart
		history: number[];
		timestamps?: number[];
		stats: Stats;
		chartColor: string;
		// Domain for chart (optional)
		domainMin?: number;
		domainMax?: number;
	}

	let {
		href = '/charts',
		shadowColor,
		sparklineId,
		title,
		icon: Icon,
		value,
		unit,
		hasError = false,
		history,
		timestamps = [],
		stats,
		chartColor,
		domainMin,
		domainMax
	}: Props = $props();

	let hoveredValue = $state<number | null>(null);
	let hoveredTime = $state<string | null>(null);
	const hasValue = $derived(value !== null);

	const formattedValue = $derived(
		value != null ? (Number.isInteger(value) ? value : value.toFixed(1)) : '--'
	);

	const formattedHoveredValue = $derived(
		hoveredValue != null
			? Number.isInteger(hoveredValue)
				? hoveredValue
				: hoveredValue.toFixed(1)
			: null
	);

	function formatTime(ts: number): string {
		const ms = ts < 100_000_000_000 ? ts * 1000 : ts;
		const d = new Date(ms);
		return new Intl.DateTimeFormat(i18n.languageTag, {
			hour: '2-digit',
			minute: '2-digit'
		}).format(d);
	}

	function handleSparklineHover(val: number | null, index: number | null) {
		hoveredValue = val;
		if (index !== null && timestamps.length > index) {
			hoveredTime = formatTime(timestamps[index]);
		} else {
			hoveredTime = null;
		}
	}
</script>

<a {href} class="card bg-base-200 card-shadow-base card-shadow-{shadowColor} cursor-pointer">
	<div class="card-body p-4 sm:p-5 pb-2 sm:pb-3">
		<!-- Header -->
		<div class="flex items-center justify-between gap-2 mb-2">
			<h2 class="card-title text-sm sm:text-base flex items-center gap-2 min-w-0">
				<Icon class="w-5 h-5 flex-shrink-0" />
				<span class="truncate">{title}</span>
				{#if hasError}
					<span
						class="tooltip tooltip-right"
						data-tip={m.dashboard_sensor_error({ locale: i18n.languageTag })}
					>
						<AlertTriangle class="w-4 h-4 text-warning animate-pulse" />
					</span>
				{/if}
			</h2>
			{#if formattedHoveredValue !== null}
				<div class="flex items-center gap-2">
					<span
						class="text-sm font-mono font-semibold px-2 py-0.5 rounded-md"
						style="background-color: color-mix(in srgb, {chartColor}, transparent 80%); color: {chartColor};"
					>
						{formattedHoveredValue}{unit}
					</span>
					{#if hoveredTime}
						<span class="text-xs text-base-content/50">{hoveredTime}</span>
					{/if}
				</div>
			{/if}
		</div>

		<!-- Value with sparkline -->
		<div
			class="flex flex-row sm:flex-col xl:flex-row items-center sm:items-start xl:items-center gap-2 sm:gap-1 xl:gap-3 min-w-0"
		>
			<div class="flex items-baseline gap-1 flex-shrink-0">
				<p class="text-2xl sm:text-3xl lg:text-2xl xl:text-3xl font-bold tabular-nums">
					{formattedValue}
					{#if hasValue}
						<span class="text-xs sm:text-sm font-normal">{unit}</span>
					{/if}
				</p>
			</div>
			<div class="flex-1 w-full min-w-0 h-[48px] opacity-70 hover:opacity-100 transition-opacity">
				{#if history.length > 1}
					<Sparkline
						id={sparklineId}
						data={history}
						width={280}
						height={48}
						color={chartColor}
						strokeWidth={2}
						{domainMin}
						{domainMax}
						smoothingTension={4}
						onHover={handleSparklineHover}
					/>
				{:else if !hasValue}
					<div class="flex h-full items-center text-xs text-base-content/55">
						{m.dashboard_waiting_first_read({ locale: i18n.languageTag })}
					</div>
				{/if}
			</div>
		</div>

		<!-- Statistics row -->
		<div
			class="flex flex-wrap items-center justify-between gap-x-4 gap-y-1 text-xs text-base-content/60 mt-2 pt-2 border-t border-base-content/10 min-h-[33px]"
			class:invisible={history.length <= 1}
		>
			{#if history.length > 1}
				<span class="flex items-center gap-1">
					<span class="opacity-50">{m.stats_min_label({ locale: i18n.languageTag })}</span>
					<span class="tabular-nums"
						>{Number.isInteger(stats.min) ? stats.min : stats.min.toFixed(1)}{unit === 'ppm'
							? ''
							: unit.replace('°C', '°').replace('%', '%')}</span
					>
				</span>
				<span class="flex items-center gap-1">
					<span class="opacity-50">{m.stats_avg_label({ locale: i18n.languageTag })}</span>
					<span class="tabular-nums"
						>{Number.isInteger(stats.avg) ? stats.avg : stats.avg.toFixed(1)}{unit === 'ppm'
							? ''
							: unit.replace('°C', '°').replace('%', '%')}</span
					>
				</span>
				<span class="flex items-center gap-1">
					<span class="opacity-50">{m.stats_max_label({ locale: i18n.languageTag })}</span>
					<span class="tabular-nums"
						>{Number.isInteger(stats.max) ? stats.max : stats.max.toFixed(1)}{unit === 'ppm'
							? ''
							: unit.replace('°C', '°').replace('%', '%')}</span
					>
				</span>
			{/if}
		</div>
	</div>
</a>
