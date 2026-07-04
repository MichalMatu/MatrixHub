import {
	CSI_WATERFALL_HISTORY,
	createCsiWaterfallPalette,
	updateWaterfallPixels
} from './csiWaterfallRaster';

interface CsiWaterfallElements {
	getCanvas: () => HTMLCanvasElement | undefined;
}

const DEFAULT_SUBCARRIERS = 64;

export function useCsiWaterfallChart(
	getAmplitudes: () => Float32Array,
	elements: CsiWaterfallElements
) {
	let ctx: CanvasRenderingContext2D | null = null;
	let rasterCanvas: HTMLCanvasElement | null = null;
	let rasterCtx: CanvasRenderingContext2D | null = null;
	let imageData: ImageData | null = null;
	let resizeObserver: ResizeObserver | undefined;
	let rasterWidth = 0;
	const palette = createCsiWaterfallPalette();

	function getCanvas() {
		return elements.getCanvas();
	}

	function ensureContexts() {
		const canvas = getCanvas();

		if (!ctx && canvas) {
			ctx = canvas.getContext('2d');
			if (ctx) {
				ctx.imageSmoothingEnabled = false;
			}
		}

		if (!rasterCanvas && typeof document !== 'undefined') {
			rasterCanvas = document.createElement('canvas');
		}

		if (rasterCanvas && !rasterCtx) {
			rasterCtx = rasterCanvas.getContext('2d', { willReadFrequently: true });
		}
	}

	function redraw() {
		const canvas = getCanvas();
		if (!ctx || !canvas || !rasterCanvas) return;

		ctx.clearRect(0, 0, canvas.width, canvas.height);
		ctx.drawImage(rasterCanvas, 0, 0, canvas.width, canvas.height);
	}

	function syncCanvasSize() {
		const canvas = getCanvas();
		if (!canvas) return;

		ensureContexts();

		const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;
		const width = Math.max(Math.round(canvas.clientWidth * dpr), 1);
		const height = Math.max(Math.round(canvas.clientHeight * dpr), 1);

		if (canvas.width !== width || canvas.height !== height) {
			canvas.width = width;
			canvas.height = height;
		}

		if (ctx) {
			ctx.imageSmoothingEnabled = false;
		}

		redraw();
	}

	function ensureRaster(subcarriers: number) {
		ensureContexts();
		if (!rasterCanvas || !rasterCtx) return;

		const nextWidth = Math.max(subcarriers, 1);
		if (imageData && rasterWidth === nextWidth) return;

		rasterWidth = nextWidth;
		rasterCanvas.width = rasterWidth;
		rasterCanvas.height = CSI_WATERFALL_HISTORY;
		imageData = rasterCtx.createImageData(rasterWidth, CSI_WATERFALL_HISTORY);
		imageData.data.fill(0);
		rasterCtx.putImageData(imageData, 0, 0);
		redraw();
	}

	function update(amplitudes: Float32Array) {
		if (!getCanvas() || amplitudes.length === 0) return;

		ensureRaster(amplitudes.length);
		if (!rasterCtx || !imageData) return;

		updateWaterfallPixels(imageData.data, rasterWidth, CSI_WATERFALL_HISTORY, amplitudes, palette);
		rasterCtx.putImageData(imageData, 0, 0);
		redraw();
	}

	function init() {
		ensureContexts();
		ensureRaster(getAmplitudes().length || DEFAULT_SUBCARRIERS);
		syncCanvasSize();

		const canvas = getCanvas();
		if (typeof ResizeObserver !== 'undefined' && canvas) {
			resizeObserver = new ResizeObserver(() => {
				syncCanvasSize();
			});
			resizeObserver.observe(canvas);
		}
	}

	function destroy() {
		resizeObserver?.disconnect();
		resizeObserver = undefined;
		ctx = null;
		rasterCtx = null;
		rasterCanvas = null;
		imageData = null;
		rasterWidth = 0;
	}

	return {
		init,
		update,
		syncCanvasSize,
		destroy
	};
}
