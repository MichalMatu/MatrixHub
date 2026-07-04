import type uPlotNamespace from 'uplot';

interface CsiAmplitudeChartElements {
	mount: HTMLDivElement;
}

function getDefaultXRange(length: number): [number, number] {
	return [-0.5, Math.max(length - 0.5, -0.5)];
}

function getDefaultYRange(series: ArrayLike<number>[]): [number, number] {
	let maxValue = 0;

	for (const values of series) {
		for (let index = 0; index < values.length; index++) {
			if (values[index] > maxValue) {
				maxValue = values[index];
			}
		}
	}

	return [0, Math.max(100, maxValue + 10)];
}

function rangesMatch(
	currentMin: number | null | undefined,
	currentMax: number | null | undefined,
	expected: [number, number],
	epsilon: number = 0.001
) {
	if (currentMin == null || currentMax == null) return false;
	return (
		Math.abs(currentMin - expected[0]) <= epsilon && Math.abs(currentMax - expected[1]) <= epsilon
	);
}

export function useCsiAmplitudeChart(
	getAmplitudes: () => Float32Array,
	elements: CsiAmplitudeChartElements,
	uPlotLib: typeof uPlotNamespace
) {
	let chart: uPlotNamespace | undefined;
	let resizeObserver: ResizeObserver | undefined;
	let ghost1 = new Float32Array(0);
	let ghost2 = new Float32Array(0);
	let frameSkip = 0;
	let legendIndex = $state('--');
	let legendAmp = $state('--');
	let hasUserXZoom = false;
	const xAxisCache = new Map<number, number[]>();
	const gridLines = [0, 20, 40, 60, 80, 100, 120, 140, 160, 180];

	function getXAxis(length: number) {
		const cached = xAxisCache.get(length);
		if (cached) return cached;

		const axis = Array.from({ length }, (_, index) => index);
		xAxisCache.set(length, axis);
		return axis;
	}

	function createOptions(width: number): uPlotNamespace.Options {
		return {
			width,
			height: 190,
			padding: [10, 0, 0, 0],
			cursor: {
				points: { size: 8, fill: 'white' },
				drag: { x: true, y: false }
			},
			legend: { show: false },
			hooks: {
				setScale: [
					(plot, key) => {
						if (key !== 'x') return;

						const xSeries = plot.data[0] as number[];
						const defaultX = getDefaultXRange(xSeries.length);

						hasUserXZoom = !rangesMatch(plot.scales.x.min, plot.scales.x.max, defaultX);
					}
				],
				setCursor: [
					(plot) => {
						const index = plot.cursor?.idx;
						if (index == null || index < 0 || !plot.data[0] || !plot.data[3]) return;

						const xValue = plot.data[0][index];
						const amplitudeValue = plot.data[3][index];

						if (xValue !== undefined) legendIndex = xValue.toString();
						if (amplitudeValue != null) legendAmp = amplitudeValue.toFixed(1);
					}
				]
			},
			scales: {
				x: {
					time: false,
					range: (_plot: uPlotNamespace, min: number, max: number) => [min - 0.5, max + 0.5]
				},
				y: {
					auto: true,
					range: (_plot: uPlotNamespace, _min: number, max: number) => [0, Math.max(100, max + 10)]
				}
			},
			series: [
				{},
				{ stroke: 'rgba(0, 186, 85, 0.15)', width: 1, points: { show: false } },
				{ stroke: 'rgba(0, 186, 85, 0.4)', width: 1, points: { show: false } },
				{
					stroke: '#00ba55',
					width: 2,
					fill: (plot, _seriesIndex) => {
						const gradient = plot.ctx.createLinearGradient(
							0,
							plot.bbox.top,
							0,
							plot.bbox.top + plot.bbox.height
						);
						gradient.addColorStop(0, 'rgba(0, 186, 85, 0.25)');
						gradient.addColorStop(1, 'rgba(0, 186, 85, 0.0)');
						return gradient;
					},
					points: { show: false }
				}
			],
			axes: [
				{
					stroke: '#ccc',
					grid: { stroke: '#333', width: 1 },
					ticks: { stroke: '#333' },
					size: 30,
					splits: gridLines
				},
				{ show: false }
			]
		};
	}

	function render() {
		if (!elements.mount || !uPlotLib) return;

		const amplitudes = getAmplitudes();
		const initialLength = amplitudes.length > 0 ? amplitudes.length : 64;
		const xAxis = getXAxis(initialLength);
		const emptySeries = new Array(initialLength).fill(0);

		chart?.destroy();
		hasUserXZoom = false;
		chart = new uPlotLib(
			createOptions(elements.mount.clientWidth),
			[xAxis, emptySeries, emptySeries, emptySeries] as uPlotNamespace.AlignedData,
			elements.mount
		);
	}

	function update(currentAmplitudes: Float32Array) {
		if (!chart || currentAmplitudes.length === 0) return;

		if (ghost1.length !== currentAmplitudes.length) {
			ghost1 = new Float32Array(currentAmplitudes.length);
			ghost2 = new Float32Array(currentAmplitudes.length);
		}

		frameSkip++;
		if (frameSkip > 4) {
			ghost2.set(ghost1);
			ghost1.set(currentAmplitudes);
			frameSkip = 0;
		}

		const nextData = [
			getXAxis(currentAmplitudes.length),
			ghost2,
			ghost1,
			currentAmplitudes
		] as uPlotNamespace.AlignedData;

		if (!hasUserXZoom) {
			chart.setData(nextData, true);
			return;
		}

		const nextYRange = getDefaultYRange([ghost2, ghost1, currentAmplitudes]);

		chart.batch(() => {
			chart!.setData(nextData, false);
			chart!.setScale('y', { min: nextYRange[0], max: nextYRange[1] });
		});
	}

	function setupResizeObserver() {
		if (!elements.mount || typeof ResizeObserver === 'undefined') return;

		resizeObserver = new ResizeObserver(() => {
			if (!chart || !elements.mount) return;
			chart.setSize({ width: elements.mount.clientWidth, height: 190 });
		});
		resizeObserver.observe(elements.mount);
	}

	function getPlotBounds() {
		if (!chart || chart.bbox.width <= 0) return undefined;

		const rootBounds = chart.root.getBoundingClientRect();
		return {
			left: rootBounds.left + chart.bbox.left,
			width: chart.bbox.width
		};
	}

	function destroy() {
		chart?.destroy();
		chart = undefined;
		resizeObserver?.disconnect();
		resizeObserver = undefined;
	}

	return {
		get legendIndex() {
			return legendIndex;
		},
		get legendAmp() {
			return legendAmp;
		},
		render,
		update,
		setupResizeObserver,
		getPlotBounds,
		destroy
	};
}
