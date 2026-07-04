import { describe, expect, it } from 'vitest';
import { useCsiAmplitudeChart } from './useCsiAmplitudeChart.svelte';

class MockUPlot {
	static lastInstance: MockUPlot | undefined;

	scales = {
		x: { min: -0.5, max: 63.5 },
		y: { min: 0, max: 100 }
	};
	bbox = { left: 40, top: 10, width: 400, height: 150 };
	root = {
		getBoundingClientRect: () => ({ left: 100 })
	};
	data: unknown;
	setDataCalls: boolean[] = [];
	setScaleCalls: Array<{ key: string; min: number; max: number }> = [];

	constructor(
		public options: {
			hooks?: {
				setScale?: Array<(plot: MockUPlot, key: string) => void>;
			};
		},
		data: unknown,
		_mount: unknown
	) {
		this.data = data;
		MockUPlot.lastInstance = this;
	}

	setData(data: unknown, resetScales = true) {
		this.data = data;
		this.setDataCalls.push(resetScales);
	}

	setScale(key: string, limits: { min: number; max: number }) {
		this.setScaleCalls.push({ key, min: limits.min, max: limits.max });
	}

	batch(fn: () => void) {
		fn();
	}

	setSize(_size: { width: number; height: number }) {}

	destroy() {}
}

describe('useCsiAmplitudeChart', () => {
	it('keeps auto-resetting scales while chart is not zoomed', () => {
		$effect.root(() => {
			const amplitudes = new Float32Array(64).fill(12);
			const chart = useCsiAmplitudeChart(
				() => amplitudes,
				{ mount: { clientWidth: 480 } as HTMLDivElement },
				MockUPlot as never
			);

			chart.render();
			chart.update(amplitudes);

			expect(MockUPlot.lastInstance?.setDataCalls.at(-1)).toBe(true);
		});
	});

	it('preserves user zoom on live updates', () => {
		$effect.root(() => {
			const amplitudes = new Float32Array(64).fill(12);
			const chart = useCsiAmplitudeChart(
				() => amplitudes,
				{ mount: { clientWidth: 480 } as HTMLDivElement },
				MockUPlot as never
			);

			chart.render();

			MockUPlot.lastInstance!.scales.x.min = 10;
			MockUPlot.lastInstance!.scales.x.max = 20;
			MockUPlot.lastInstance!.options.hooks?.setScale?.forEach((hook) =>
				hook(MockUPlot.lastInstance!, 'x')
			);

			chart.update(amplitudes);

			expect(MockUPlot.lastInstance?.setDataCalls.at(-1)).toBe(false);
			expect(MockUPlot.lastInstance?.setScaleCalls.at(-1)?.key).toBe('y');
		});
	});

	it('exposes the uPlot data area for band-selection coordinates', () => {
		$effect.root(() => {
			const amplitudes = new Float32Array(64).fill(12);
			const chart = useCsiAmplitudeChart(
				() => amplitudes,
				{ mount: { clientWidth: 480 } as HTMLDivElement },
				MockUPlot as never
			);

			chart.render();

			expect(chart.getPlotBounds()).toEqual({ left: 140, width: 400 });
		});
	});
});
