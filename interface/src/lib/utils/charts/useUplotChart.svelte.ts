import { onMount, onDestroy } from 'svelte';
import type uPlotNamespace from 'uplot';
import type { AlignedData, Options } from 'uplot';

/**
 * Global cache for the uPlot library to avoid redundant loads
 */
let uPlotLib: typeof uPlotNamespace | null = null;
let isLibraryLoading = false;

/**
 * Dynamically loads uPlot and its styles.
 * Returns the uPlot constructor.
 */
export async function loadUplotLib() {
	if (uPlotLib) return uPlotLib;
	if (isLibraryLoading) {
		// Wait if another component is already loading it
		while (isLibraryLoading) {
			await new Promise((resolve) => setTimeout(resolve, 50));
		}
		return uPlotLib;
	}

	isLibraryLoading = true;
	try {
		// Import JS and CSS dynamically
		const [uPlotModule] = await Promise.all([import('uplot'), import('uplot/dist/uPlot.min.css')]);
		uPlotLib = uPlotModule.default;
		return uPlotLib;
	} catch (e) {
		console.error('[uPlot] Failed to load library', e);
		throw e;
	} finally {
		isLibraryLoading = false;
	}
}

import {
	commonCursorConfig,
	createSingleSeriesConfig,
	createSingleAxesConfig,
	chartSyncManager,
	wheelZoomPlugin
} from '$lib/utils/charts';

interface UplotChartConfig {
	chartId: string;
	title: string;
	color: string;
	unit: string;
	decimals: number;
	yRange?: [number, number] | ((u: uPlotNamespace, min: number, max: number) => [number, number]);
	minPadding?: number;
	yFloor?: number;
}

export interface UplotChartState {
	shouldScroll: boolean;
	isZoomed: boolean;
	selectedPoint: { value: number; timestamp: number } | null;
	compact: boolean;
	ultraCompact: boolean;
}

const SCROLL_TRIGGER_WIDTH = 550;
const SCROLL_MIN_PLOT_WIDTH = 550;

export function useUplotChart(configFn: () => UplotChartConfig) {
	let chart: uPlotNamespace | undefined;
	let chartViewport: HTMLElement | undefined;
	let chartMount: HTMLElement | undefined;
	let lastHeight = 250;
	let initialXRange: { min: number; max: number } | null = null;
	let resizeObserver: ResizeObserver | undefined;
	let resizeTimer: ReturnType<typeof setTimeout> | undefined;

	let state = $state<UplotChartState>({
		shouldScroll: false,
		isZoomed: false,
		selectedPoint: null,
		compact: false,
		ultraCompact: false
	});

	function renderChart(data: (number | null)[], timestamps: number[]) {
		if (!chartViewport || !chartMount || data.length === 0 || timestamps.length === 0 || !uPlotLib)
			return;

		if (chart) chart.destroy();

		const plotData = [timestamps, data] as AlignedData;
		const viewportWidth = chartViewport.clientWidth;

		state.compact = viewportWidth < 360;
		state.ultraCompact = viewportWidth < 320;
		const isDesktop = viewportWidth >= 1024;
		state.shouldScroll = viewportWidth < SCROLL_TRIGGER_WIDTH;

		const plotWidth = state.shouldScroll
			? Math.max(SCROLL_MIN_PLOT_WIDTH, viewportWidth - 2) // Slight reduction for borders
			: viewportWidth - 2;

		chartMount.style.width = `${plotWidth}px`;
		lastHeight = state.ultraCompact ? 190 : state.compact ? 220 : isDesktop ? 350 : 280;

		// Zwiększenie prawego paddingu zapobiega ucinaniu tekstów osi X
		const padding: [number, number, number, number] = state.ultraCompact
			? [4, 15, 0, 0]
			: state.compact
				? [6, 18, 0, 0]
				: [8, 20, 0, 0];

		const config = configFn();

		const opts: Options = {
			width: plotWidth,
			height: lastHeight,
			padding,
			cursor: commonCursorConfig,
			plugins: [wheelZoomPlugin()],
			scales: {
				x: { time: true },
				y: config.yRange
					? { auto: false, range: config.yRange }
					: {
							auto: true,
							range: (u, dataMin, dataMax) => {
								const span = dataMax - dataMin;
								const minPad = config.minPadding ?? 1;
								let pad = Math.max(span * 0.5, minPad);
								if (span === 0) pad = Math.max(dataMin * 0.1, minPad);
								const lo =
									config.yFloor != null ? Math.max(config.yFloor, dataMin - pad) : dataMin - pad;
								return [lo, dataMax + pad];
							}
						}
			},
			series: createSingleSeriesConfig(
				uPlotLib,
				config.title,
				config.color,
				config.unit,
				config.decimals
			),
			axes: createSingleAxesConfig(config.unit, config.decimals, undefined, {
				compact: state.compact,
				tiny: state.shouldScroll
			}),
			legend: { show: false }
		};

		if (opts.axes && opts.axes[0]) {
			opts.axes[0].space = state.compact ? 90 : state.shouldScroll ? 70 : 50;
		}

		chart = new uPlotLib(opts, plotData, chartMount);

		initialXRange = {
			min: timestamps[0],
			max: timestamps[timestamps.length - 1]
		};
		state.isZoomed = false;

		setupZoomTracking();
		setupCursorSync(data, timestamps);
		setupDoubleClickReset();
	}

	function setupZoomTracking() {
		if (!chart) return;

		if (!chart.hooks.setScale) {
			chart.hooks.setScale = [];
		}

		chart.hooks.setScale.push((u, scaleKey) => {
			if (scaleKey === 'x' && initialXRange) {
				const currentMin = u.scales.x ? u.scales.x.min! : 0;
				const currentMax = u.scales.x ? u.scales.x.max! : 0;
				const initialRange = initialXRange.max - initialXRange.min;
				const currentRange = currentMax - currentMin;
				const rangeDiff = Math.abs(currentRange - initialRange) / initialRange;
				const isFullRange = rangeDiff < 0.01;
				state.isZoomed = !isFullRange;
			}
		});
	}

	function setupCursorSync(data: (number | null)[], timestamps: number[]) {
		if (!chart) return;
		const config = configFn();

		chartSyncManager.register(config.chartId, chart);

		chart.over.addEventListener('mousemove', () => {
			if (!chart) return;
			const { left, top, idx } = chart.cursor;
			if (left !== undefined && top !== undefined) {
				chartSyncManager.syncCursor(config.chartId, left, top);
			}

			if (idx !== undefined && idx !== null && idx >= 0 && idx < data.length) {
				const value = data[idx];
				const ts = timestamps[idx];
				if (value !== null && !isNaN(value)) {
					state.selectedPoint = { value, timestamp: ts };
				}
			}
		});

		chart.over.addEventListener('mouseleave', () => {
			chartSyncManager.clearCursors(configFn().chartId);
			state.selectedPoint = null;
		});
	}

	function setupDoubleClickReset() {
		if (!chart) return;
		chart.over.addEventListener('dblclick', () => {
			resetZoom();
		});
	}

	function setupResizeObserver() {
		if (chartViewport && typeof ResizeObserver !== 'undefined') {
			resizeObserver = new ResizeObserver(() => {
				// Debounce resize to avoid excessive redraws (especially on mobile/ESP)
				clearTimeout(resizeTimer);
				resizeTimer = setTimeout(() => {
					if (chart && chartViewport && chartMount) {
						const viewportWidth = chartViewport.clientWidth;
						state.compact = viewportWidth < 360;
						state.ultraCompact = viewportWidth < 320;
						const isDesktop = viewportWidth >= 1024;
						state.shouldScroll = viewportWidth < SCROLL_TRIGGER_WIDTH;

						const plotWidth = state.shouldScroll
							? Math.max(SCROLL_MIN_PLOT_WIDTH, viewportWidth - 2) // Slight reduction for borders
							: viewportWidth - 2;

						chartMount.style.width = `${plotWidth}px`;
						const height = state.ultraCompact ? 190 : state.compact ? 220 : isDesktop ? 350 : 280;
						chart.setSize({ width: plotWidth, height });
					}
				}, 150);
			});
			resizeObserver.observe(chartViewport);
		}
	}

	function resetZoom() {
		if (chart && initialXRange) {
			state.isZoomed = false;
			const c = chart;
			const xRange = initialXRange;
			c.batch(() => {
				c.setScale('x', { min: xRange.min, max: xRange.max });
			});
		}
	}

	function cleanup() {
		chartSyncManager.unregister(configFn().chartId);
		clearTimeout(resizeTimer);
		if (chart) chart.destroy();
		if (resizeObserver) resizeObserver.disconnect();
	}

	function mount(viewport: HTMLElement, mount: HTMLElement) {
		chartViewport = viewport;
		chartMount = mount;
	}

	onMount(() => {
		setupResizeObserver();
	});

	onDestroy(() => {
		cleanup();
	});

	return {
		get state() {
			return state;
		},
		get isLibraryLoaded() {
			return !!uPlotLib;
		},
		renderChart,
		resetZoom,
		mount
	};
}
