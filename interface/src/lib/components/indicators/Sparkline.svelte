<script lang="ts">
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { calculateSparklineGeometry } from './useSparklineGeometry';
	import { useSparklineHover } from './useSparklineHover.svelte';

	interface Props {
		id: string;
		data: number[];
		width?: number;
		height?: number;
		color?: string;
		strokeWidth?: number;
		domainMin?: number;
		domainMax?: number;
		padding?: number;
		showFill?: boolean;
		showEndpoint?: boolean;
		animate?: boolean;
		smooth?: boolean;
		smoothingTension?: number;
		onHover?: (_value: number | null, _index: number | null) => void;
	}

	let {
		id,
		data,
		width = 80,
		height = 40,
		color = 'currentColor',
		strokeWidth = 2,
		domainMin,
		domainMax,
		padding,
		showFill = true,
		showEndpoint = true,
		animate = true,
		smooth = true,
		smoothingTension = 4,
		onHover
	}: Props = $props();

	let svgElement: SVGSVGElement;

	// Generate unique ID for gradient
	const gradientId = $derived(`sparkline-gradient-${id}`);

	// Calculate geometry from data
	const chartData = $derived(
		calculateSparklineGeometry({
			data,
			width,
			height,
			strokeWidth,
			domainMin,
			domainMax,
			padding,
			smoothingTension
		})
	);

	// Hover interaction state
	const hoverState = useSparklineHover(
		() => onHover,
		() => data
	);
</script>

<svg
	bind:this={svgElement}
	{width}
	{height}
	viewBox="0 0 {width} {height}"
	class="inline-block w-full h-auto max-w-full sparkline-svg cursor-crosshair"
	class:animate-fade-in={animate}
	preserveAspectRatio="xMaxYMid meet"
	role="img"
	aria-label={m.aria_sparkline({ locale: i18n.languageTag })}
	onmousemove={(e) => hoverState.handleMouseMove(e, svgElement, data.length)}
	onmouseleave={hoverState.handleMouseLeave}
>
	<defs>
		<linearGradient id={gradientId} x1="0%" y1="0%" x2="0%" y2="100%">
			<stop offset="0%" stop-color={color} stop-opacity="0.3" />
			<stop offset="100%" stop-color={color} stop-opacity="0.02" />
		</linearGradient>
	</defs>

	<!-- Gradient fill under the line -->
	{#if showFill}
		{#if smooth && chartData.smoothFillPath}
			<path d={chartData.smoothFillPath} fill="url(#{gradientId})" class="sparkline-fill" />
		{:else if chartData.fillPoints}
			<polygon points={chartData.fillPoints} fill="url(#{gradientId})" class="sparkline-fill" />
		{/if}
	{/if}

	<!-- Main line -->
	{#if smooth && chartData.smoothPath}
		<path
			d={chartData.smoothPath}
			fill="none"
			stroke={color}
			stroke-width={strokeWidth}
			stroke-linecap="round"
			stroke-linejoin="round"
			vector-effect="non-scaling-stroke"
			class="sparkline-line"
		/>
	{:else}
		<polyline
			points={chartData.points}
			fill="none"
			stroke={color}
			stroke-width={strokeWidth}
			stroke-linecap="round"
			stroke-linejoin="round"
			vector-effect="non-scaling-stroke"
			class="sparkline-line"
		/>
	{/if}

	<!-- Endpoint dot -->
	{#if showEndpoint && chartData.lastPoint}
		{@const lastPt = chartData.lastPoint}
		<circle
			cx={lastPt.x}
			cy={lastPt.y}
			r={strokeWidth + 1}
			fill={color}
			class="sparkline-endpoint"
		/>
		<!-- Pulsing ring around endpoint -->
		<circle
			cx={lastPt.x}
			cy={lastPt.y}
			r={strokeWidth + 3}
			fill="none"
			stroke={color}
			stroke-width="1"
			opacity="0.5"
			class="sparkline-pulse"
		/>
	{/if}
</svg>

<style>
	.sparkline-svg {
		overflow: visible;
	}

	.animate-fade-in {
		animation: fadeIn 0.4s ease-out;
	}

	@keyframes fadeIn {
		from {
			opacity: 0;
			transform: translateY(4px);
		}
		to {
			opacity: 1;
			transform: translateY(0);
		}
	}

	.sparkline-line {
		transition: stroke-dashoffset 0.3s ease-out;
	}

	.sparkline-fill {
		transition: opacity 0.3s ease-out;
	}

	.sparkline-endpoint {
		filter: drop-shadow(0 0 2px currentColor);
	}

	.sparkline-pulse {
		animation: pulse 2s ease-in-out infinite;
	}

	@keyframes pulse {
		0%,
		100% {
			opacity: 0.5;
			r: 4;
		}
		50% {
			opacity: 0.2;
			r: 7;
		}
	}
</style>
