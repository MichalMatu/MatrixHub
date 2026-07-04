<script lang="ts">
	import { onMount } from 'svelte';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import { loadUplotLib } from '$lib/utils/charts/useUplotChart.svelte';
	import ChartLine from '~icons/tabler/chart-line';
	import Wifi from '~icons/tabler/wifi';
	import Antenna from '~icons/tabler/antenna';
	import { Spinner } from '$lib/components';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import type { CsiAlarmBand } from '$lib/types/connectivity/wifiSensing';
	import * as m from '$lib/paraglide/messages.js';
	import { useCsiAmplitudeChart } from './useCsiAmplitudeChart.svelte';

	interface Props {
		amplitudes: Float32Array;
		subcarriers: number;
		rssi: number;
		gain: number;
		isConnected: boolean;
		userEnabled: boolean;
		bands?: CsiAlarmBand[];
		selectionMode?: boolean;
		onToggleConnection: () => void;
		onBandSelected?: (band: CsiAlarmBand) => void;
	}

	let {
		amplitudes,
		subcarriers,
		rssi,
		gain,
		isConnected,
		userEnabled,
		bands = [],
		selectionMode = false,
		onToggleConnection,
		onBandSelected
	}: Props = $props();

	let canvasAmp: HTMLDivElement;
	let selectionLayer = $state<HTMLDivElement | undefined>(undefined);
	let chart = $state<ReturnType<typeof useCsiAmplitudeChart> | undefined>(undefined);
	let isLoading = $state(true);
	let helpOpen = $state(false);
	let dragStartIndex = $state<number | null>(null);
	let dragEndIndex = $state<number | null>(null);
	const locale = $derived(i18n.languageTag);
	const carrierCount = $derived(Math.max(subcarriers || amplitudes.length || 1, 1));
	const draftBand = $derived.by(() => {
		if (dragStartIndex == null || dragEndIndex == null) return null;
		const start = Math.min(dragStartIndex, dragEndIndex);
		const end = Math.max(dragStartIndex, dragEndIndex);
		return { start, end };
	});
	const streamStateLabel = $derived(
		isConnected
			? m.csi_status_live()
			: userEnabled
				? m.csi_status_connecting()
				: m.csi_status_paused()
	);
	const streamStateClass = $derived(
		isConnected ? 'badge-success' : userEnabled ? 'badge-warning' : 'badge-ghost'
	);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.csi_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.csi_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.csi_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/wifisensing', label: m.menu_wifisensing({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);

	function bandStyle(band: CsiAlarmBand) {
		const start = Math.max(0, Math.min(carrierCount - 1, band.start));
		const end = Math.max(0, Math.min(carrierCount - 1, band.end));
		const left = (Math.min(start, end) / carrierCount) * 100;
		const width = ((Math.abs(end - start) + 1) / carrierCount) * 100;
		return `left:${left}%;width:${width}%`;
	}

	function pointerIndex(event: PointerEvent) {
		if (!selectionLayer) return 0;
		const rect = selectionLayer.getBoundingClientRect();
		const plotBounds = chart?.getPlotBounds();
		const plotLeft = plotBounds?.left ?? rect.left;
		const plotWidth = plotBounds?.width ?? rect.width;
		const ratio = Math.max(0, Math.min(1, (event.clientX - plotLeft) / Math.max(plotWidth, 1)));
		return Math.round(ratio * (carrierCount - 1));
	}

	function handleSelectionStart(event: PointerEvent) {
		if (!selectionMode) return;
		const index = pointerIndex(event);
		dragStartIndex = index;
		dragEndIndex = index;
		selectionLayer?.setPointerCapture(event.pointerId);
		event.preventDefault();
	}

	function handleSelectionMove(event: PointerEvent) {
		if (!selectionMode || dragStartIndex == null) return;
		dragEndIndex = pointerIndex(event);
		event.preventDefault();
	}

	function handleSelectionEnd(event: PointerEvent) {
		if (!selectionMode || dragStartIndex == null || dragEndIndex == null) return;
		const start = Math.min(dragStartIndex, dragEndIndex);
		const end = Math.max(dragStartIndex, dragEndIndex);
		onBandSelected?.({ start, end });
		dragStartIndex = null;
		dragEndIndex = null;
		selectionLayer?.releasePointerCapture(event.pointerId);
		event.preventDefault();
	}

	// Watch for data updates
	$effect(() => {
		if (amplitudes.length > 0 && chart) {
			chart.update(amplitudes);
		}
	});

	$effect(() => {
		if (!selectionMode) {
			dragStartIndex = null;
			dragEndIndex = null;
		}
	});

	onMount(() => {
		let isDisposed = false;

		void (async () => {
			try {
				const uPlot = await loadUplotLib();
				if (!uPlot || isDisposed) return;

				chart = useCsiAmplitudeChart(() => amplitudes, { mount: canvasAmp }, uPlot);
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
</script>

<BaseCard title={m.csi_amplitudes_title()} icon={ChartLine} class="h-full">
	{#snippet actions()}
		<div class="flex flex-wrap gap-2 sm:gap-4 text-xs items-center justify-end mr-1">
			<HelpTriggerButton label={m.csi_help_title({ locale })} onclick={() => (helpOpen = true)} />

			<div class="hidden xs:flex gap-2 font-mono border-r border-base-content/20 pr-2 sm:pr-4 mr-2">
				<div class="hidden sm:block">
					{m.csi_index_label()}:
					<span class="font-bold inline-block w-[3ch] text-right">{chart?.legendIndex ?? '--'}</span
					>
				</div>
				<div class="flex items-center gap-1">
					<div class="w-2 h-2 rounded-full bg-[#00ba55]"></div>
					<span class="hidden sm:inline">{m.csi_amplitude_short_label()}:</span><span
						class="font-bold w-[4ch] sm:w-[5ch] text-right">{chart?.legendAmp ?? '--'}</span
					>
				</div>
			</div>

			<div class="hidden lg:flex items-center gap-3 border-r border-base-content/20 pr-4 mr-2">
				<div class="flex items-center gap-1" title={m.csi_signal_strength_title()}>
					<Antenna class="w-4 h-4 opacity-70" />
					<span class="font-mono">{rssi}dBm</span>
				</div>
				<div class="opacity-50 font-mono text-[10px]">
					{m.csi_gain_label()}:{gain.toFixed(1)}
				</div>
				<div class="opacity-50 font-mono text-[10px]">
					{subcarriers}{m.csi_subcarriers_suffix()}
				</div>
				<div class="badge badge-xs {streamStateClass} uppercase">{streamStateLabel}</div>
			</div>

			<div class="flex items-center gap-2" title={m.csi_toggle_connection_title()}>
				<Wifi class="w-4 h-4 {isConnected ? 'text-success' : 'opacity-50'}" />
				<input
					type="checkbox"
					class="toggle toggle-success toggle-sm"
					checked={userEnabled}
					aria-label={m.csi_toggle_connection_title()}
					onchange={onToggleConnection}
				/>
			</div>
		</div>
	{/snippet}
	<div bind:this={canvasAmp} class="relative w-full h-[190px]">
		{#if isLoading}
			<div
				class="absolute inset-0 flex items-center justify-center bg-base-100/30 z-20 backdrop-blur-[1px]"
			>
				<Spinner />
			</div>
		{/if}
		{#each bands as band}
			<div
				class="pointer-events-none absolute top-2 bottom-8 z-10 border-x border-info/70 bg-info/15"
				style={bandStyle(band)}
			></div>
		{/each}
		{#if selectionMode}
			<div
				bind:this={selectionLayer}
				role="application"
				aria-label={m.csi_alarm_select_band()}
				class="absolute inset-0 z-30 cursor-crosshair touch-none"
				onpointerdown={handleSelectionStart}
				onpointermove={handleSelectionMove}
				onpointerup={handleSelectionEnd}
				onpointercancel={() => {
					dragStartIndex = null;
					dragEndIndex = null;
				}}
			>
				{#if draftBand}
					<div
						class="pointer-events-none absolute top-2 bottom-8 border-x border-warning bg-warning/20"
						style={bandStyle(draftBand)}
					></div>
				{/if}
			</div>
		{/if}
	</div>

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.csi_help_title({ locale })}
		intro={m.csi_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</BaseCard>
