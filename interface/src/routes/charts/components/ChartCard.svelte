<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let {
		title,
		icon: Icon,
		chartId,
		children,
		shadowColor = 'primary',
		stats = null,
		unit = '',
		decimals = 1,
		selectedPoint = null,
		color = 'var(--color-primary, #3b82f6)'
	}: {
		title: string;
		icon: Component;
		chartId: string;
		children: Snippet;
		shadowColor?: 'warning' | 'primary' | 'error' | 'success' | 'info';
		stats?: { min: number; max: number; avg: number } | null;
		unit?: string;
		decimals?: number;
		selectedPoint?: { value: number; timestamp: number } | null;
		color?: string;
	} = $props();

	function formatTime(ts: number): string {
		const ms = ts < 100_000_000_000 ? ts * 1000 : ts;
		const d = new Date(ms);
		return new Intl.DateTimeFormat('pl-PL', {
			hour: '2-digit',
			minute: '2-digit'
		}).format(d);
	}
</script>

<BaseCard {title} icon={Icon} class="h-full card-shadow-{shadowColor}" bleed={true}>
	{#snippet actions()}
		{#if selectedPoint}
			<!-- Show selected point value prominently -->
			<div class="flex items-center gap-2">
				<span class="font-mono text-sm font-semibold" style="color: {color};">
					{selectedPoint.value.toFixed(decimals)}{unit}
				</span>
				<span class="text-xs opacity-50">
					{formatTime(selectedPoint.timestamp)}
				</span>
			</div>
		{:else if stats}
			<div class="flex gap-2 sm:gap-3 text-xs opacity-70 whitespace-nowrap">
				<span class="text-success" title={m.chart_stat_min({ locale: i18n.languageTag })}>
					↓{stats.min.toFixed(decimals)}{unit}
				</span>
				<span class="text-info" title={m.chart_stat_avg({ locale: i18n.languageTag })}>
					~{stats.avg.toFixed(decimals)}{unit}
				</span>
				<span class="text-error" title={m.chart_stat_max({ locale: i18n.languageTag })}>
					↑{stats.max.toFixed(decimals)}{unit}
				</span>
			</div>
		{/if}
	{/snippet}

	<div id={chartId} class="overflow-hidden">
		{@render children()}
	</div>
</BaseCard>
