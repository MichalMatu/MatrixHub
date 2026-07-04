import type uPlotNamespace from 'uplot';
import {
	calculateYRange,
	calculateVarianceRange,
	createChartOptions,
	getChartColor
} from './RssiChartConfig';

export interface RssiChartSample {
	rssi: number;
	timestamp: number;
	variance?: number;
}

interface RssiChartElements {
	viewport: HTMLElement;
	mount: HTMLElement;
}

const AXIS_BREAKPOINT = 950;

export function useRssiChart(
	getSamples: () => RssiChartSample[],
	getMotion: () => boolean,
	elements: RssiChartElements,
	uPlotLib: typeof uPlotNamespace
) {
	let chart: uPlotNamespace | undefined;
	let resizeObserver: ResizeObserver | undefined;
	let lastMotionState: boolean | undefined = undefined;
	let currentlyShowingRssiAxis = true;
	let currentlyShowingVarianceAxis = true;

	function getPlotData(): uPlotNamespace.AlignedData | null {
		const samples = getSamples();
		const length = samples.length;
		if (length === 0) return null;

		const timestamps = new Array<number>(length);
		const rssiValues = new Array<number>(length);
		const varianceValues = new Array<number>(length);

		for (let index = 0; index < length; index++) {
			const sample = samples[index];
			timestamps[index] = sample.timestamp / 1000;
			rssiValues[index] = sample.rssi;
			varianceValues[index] = sample.variance ?? 0;
		}

		return [timestamps, rssiValues, varianceValues] as uPlotNamespace.AlignedData;
	}

	function render() {
		if (!elements.viewport || !elements.mount || !uPlotLib) return;

		const plotData = getPlotData();
		if (!plotData) return;

		chart?.destroy();

		const viewportWidth = elements.viewport.clientWidth;
		const showRssiAxis = viewportWidth >= AXIS_BREAKPOINT;
		const showVarianceAxis = viewportWidth >= AXIS_BREAKPOINT;
		const height = elements.viewport.clientHeight;
		const motionDetected = getMotion();
		const color = getChartColor(motionDetected);
		const rssiYRange = calculateYRange(plotData[1] as number[]);
		const varianceRange = calculateVarianceRange(plotData[2] as number[]);

		currentlyShowingRssiAxis = showRssiAxis;
		currentlyShowingVarianceAxis = showVarianceAxis;
		chart = new uPlotLib(
			createChartOptions(
				viewportWidth,
				height,
				showRssiAxis,
				showVarianceAxis,
				rssiYRange,
				varianceRange,
				color,
				motionDetected
			),
			plotData,
			elements.mount
		);
		lastMotionState = motionDetected;
	}

	function update() {
		const plotData = getPlotData();
		if (!plotData) return;

		if (!chart) {
			render();
			return;
		}

		chart.setData(plotData);

		const nextRssiRange = calculateYRange(plotData[1] as number[]);
		const nextVarianceRange = calculateVarianceRange(plotData[2] as number[]);

		chart.batch(() => {
			chart!.setScale('y', { min: nextRssiRange[0], max: nextRssiRange[1] });
			chart!.setScale('y2', { min: nextVarianceRange[0], max: nextVarianceRange[1] });
		});

		const motionDetected = getMotion();
		if (lastMotionState === motionDetected) {
			return;
		}

		lastMotionState = motionDetected;

		chart.batch(() => {
			const color = getChartColor(motionDetected);
			chart!.series[1].stroke = () => color;
			chart!.series[1].fill = (plot: uPlotNamespace) => {
				const gradient = plot.ctx.createLinearGradient(0, 0, 0, plot.bbox.height);
				if (motionDetected) {
					gradient.addColorStop(0, 'rgba(245, 158, 11, 0.3)');
					gradient.addColorStop(1, 'rgba(245, 158, 11, 0)');
				} else {
					gradient.addColorStop(0, 'rgba(59, 130, 246, 0.3)');
					gradient.addColorStop(1, 'rgba(59, 130, 246, 0)');
				}
				return gradient;
			};
		});

		chart.redraw();
	}

	function setupResizeObserver() {
		if (!elements.viewport || typeof ResizeObserver === 'undefined') return;

		resizeObserver = new ResizeObserver(() => {
			if (!chart || !elements.viewport) return;

			const viewportWidth = elements.viewport.clientWidth;
			const shouldShowRssiAxis = viewportWidth >= AXIS_BREAKPOINT;
			const shouldShowVarianceAxis = viewportWidth >= AXIS_BREAKPOINT;

			if (
				shouldShowRssiAxis !== currentlyShowingRssiAxis ||
				shouldShowVarianceAxis !== currentlyShowingVarianceAxis
			) {
				render();
				return;
			}

			chart.setSize({ width: viewportWidth, height: elements.viewport.clientHeight });
		});

		resizeObserver.observe(elements.viewport);
	}

	function destroy() {
		chart?.destroy();
		chart = undefined;
		resizeObserver?.disconnect();
		resizeObserver = undefined;
	}

	return {
		render,
		update,
		setupResizeObserver,
		destroy
	};
}
