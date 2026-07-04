import { afterEach, describe, expect, it, vi } from 'vitest';
import { wheelZoomPlugin } from './wheelZoomPlugin';

function createChartStub({
	initialMin = 0,
	initialMax = 100,
	width = 200,
	cursorLeft = 100
}: {
	initialMin?: number;
	initialMax?: number;
	width?: number;
	cursorLeft?: number | undefined;
} = {}) {
	const over = document.createElement('div');
	document.body.appendChild(over);

	Object.defineProperty(over, 'getBoundingClientRect', {
		configurable: true,
		value: () => ({
			left: 0,
			top: 0,
			right: width,
			bottom: 100,
			width,
			height: 100,
			x: 0,
			y: 0,
			toJSON: () => ({})
		})
	});

	const chart = {
		scales: {
			x: {
				min: initialMin,
				max: initialMax
			}
		},
		cursor: {
			left: cursorLeft
		},
		over,
		posToVal: vi.fn((pos: number) => {
			const span = chart.scales.x.max - chart.scales.x.min;
			return chart.scales.x.min + (pos / width) * span;
		}),
		batch: vi.fn((fn: () => void) => fn()),
		setScale: vi.fn((scale: string, range: { min: number; max: number }) => {
			if (scale === 'x') {
				chart.scales.x.min = range.min;
				chart.scales.x.max = range.max;
			}
		})
	};

	return chart;
}

function dispatchWheel(node: HTMLElement, deltaY: number) {
	const event = new WheelEvent('wheel', {
		deltaY,
		bubbles: true,
		cancelable: true
	});
	node.dispatchEvent(event);
	return event;
}

function dispatchTouch(node: HTMLElement, type: string, clientXs: number[]) {
	const event = new Event(type, {
		bubbles: true,
		cancelable: true
	});

	Object.defineProperty(event, 'touches', {
		configurable: true,
		value: clientXs.map((clientX) => ({ clientX }))
	});

	node.dispatchEvent(event);
	return event;
}

function runReadyHook(
	plugin: ReturnType<typeof wheelZoomPlugin>,
	chart: ReturnType<typeof createChartStub>
) {
	const readyHook = plugin.hooks.ready;

	if (Array.isArray(readyHook)) {
		for (const hook of readyHook) {
			hook?.(chart as never);
		}
		return;
	}

	readyHook?.(chart as never);
}

afterEach(() => {
	document.body.innerHTML = '';
	vi.useRealTimers();
});

describe('wheelZoomPlugin', () => {
	it('sets touch handling and zooms in around the cursor on wheel up', () => {
		const chart = createChartStub({ initialMin: 0, initialMax: 2000, cursorLeft: 100 });
		const plugin = wheelZoomPlugin({ blockScroll: true });

		runReadyHook(plugin, chart);

		expect(chart.over.style.touchAction).toBe('pan-x pan-y');

		const event = dispatchWheel(chart.over, -1);
		const [, range] = chart.setScale.mock.calls[0] as [string, { min: number; max: number }];

		expect(event.defaultPrevented).toBe(true);
		expect(chart.setScale).toHaveBeenCalledTimes(1);
		expect(range.min).toBeCloseTo(250);
		expect(range.max).toBeCloseTo(1750);
	});

	it('throttles wheel zoom until the timeout expires', () => {
		vi.useFakeTimers();

		const chart = createChartStub({ initialMin: 0, initialMax: 2000 });
		const plugin = wheelZoomPlugin();

		runReadyHook(plugin, chart);

		dispatchWheel(chart.over, -1);
		dispatchWheel(chart.over, -1);

		expect(chart.setScale).toHaveBeenCalledTimes(1);

		vi.advanceTimersByTime(50);
		dispatchWheel(chart.over, -1);

		expect(chart.setScale).toHaveBeenCalledTimes(2);
	});

	it('respects the minimum zoom range when zooming in', () => {
		const chart = createChartStub({ initialMin: 0, initialMax: 800 });
		const plugin = wheelZoomPlugin({ minZoomSeconds: 700 });

		runReadyHook(plugin, chart);
		dispatchWheel(chart.over, -1);

		expect(chart.setScale).not.toHaveBeenCalled();
	});

	it('clamps zoom-out attempts to the original x scale domain', () => {
		const chart = createChartStub({ initialMin: 0, initialMax: 2000, cursorLeft: 100 });
		const plugin = wheelZoomPlugin();

		runReadyHook(plugin, chart);
		chart.scales.x.min = 200;
		chart.scales.x.max = 1800;

		dispatchWheel(chart.over, 1);

		const [, range] = chart.setScale.mock.calls[0] as [string, { min: number; max: number }];

		expect(range.min).toBeCloseTo(0);
		expect(range.max).toBeCloseTo(2000);
	});

	it('handles pinch zoom and resets touch state after touchend', () => {
		const chart = createChartStub({ initialMin: 0, initialMax: 2000, cursorLeft: 100 });
		const plugin = wheelZoomPlugin();

		runReadyHook(plugin, chart);

		dispatchTouch(chart.over, 'touchstart', [50, 150]);
		dispatchTouch(chart.over, 'touchmove', [0, 200]);

		const [, range] = chart.setScale.mock.calls[0] as [string, { min: number; max: number }];

		expect(chart.setScale).toHaveBeenCalledTimes(1);
		expect(range.min).toBeCloseTo(500);
		expect(range.max).toBeCloseTo(1500);

		chart.setScale.mockClear();
		dispatchTouch(chart.over, 'touchend', []);
		dispatchTouch(chart.over, 'touchmove', [25, 175]);

		expect(chart.setScale).not.toHaveBeenCalled();
	});
});
