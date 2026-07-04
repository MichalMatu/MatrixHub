/**
 * Wheel zoom plugin for uPlot charts
 * Supports mouse wheel and pinch-to-zoom on touch devices
 */
import type uPlot from 'uplot';

// Constants
const DEFAULT_ZOOM_FACTOR = 0.75;
const DEFAULT_MIN_ZOOM_RANGE_SEC = 600; // 10 minutes
const WHEEL_THROTTLE_MS = 50;

export interface WheelZoomPluginOptions {
	/**
	 * Zoom factor (default: 0.75)
	 */
	factor?: number;
	/**
	 * Minimum zoom range in seconds (default: 600 = 10 minutes)
	 */
	minZoomSeconds?: number;
	/**
	 * If true, prevent page scrolling while zooming (uses non-passive listeners).
	 * Default false to avoid console warnings about non-passive listeners.
	 */
	blockScroll?: boolean;
}

/**
 * Creates a wheel zoom plugin for uPlot
 * @param opts - Plugin options
 */
export function wheelZoomPlugin(opts: WheelZoomPluginOptions = {}): uPlot.Plugin {
	const factor = opts.factor || DEFAULT_ZOOM_FACTOR;
	const minZoomSeconds = opts.minZoomSeconds || DEFAULT_MIN_ZOOM_RANGE_SEC;
	const blockScroll = opts.blockScroll ?? false;

	let xMin: number, xMax: number, xRange: number;
	let wheelTimeout: number | undefined;

	function clamp(
		nRange: number,
		nMin: number,
		nMax: number,
		fRange: number,
		fMin: number,
		fMax: number
	): [number, number] {
		if (nRange > fRange) {
			nMin = fMin;
			nMax = fMax;
		} else if (nMin < fMin) {
			nMin = fMin;
			nMax = fMin + nRange;
		} else if (nMax > fMax) {
			nMax = fMax;
			nMin = fMax - nRange;
		}
		return [nMin, nMax];
	}

	return {
		hooks: {
			ready(u: uPlot) {
				xMin = u.scales.x.min!;
				xMax = u.scales.x.max!;
				xRange = xMax - xMin;

				const over = u.over;
				const rect = over.getBoundingClientRect();
				const wheelListenerOpts: AddEventListenerOptions = blockScroll
					? { passive: false }
					: { passive: true };
				const touchListenerOpts: AddEventListenerOptions = { passive: true };

				// Allow panning scroll (x/y), but disable browser pinch-zoom so we can handle it.
				over.style.touchAction = 'pan-x pan-y';

				// Mouse wheel zoom with throttling
				over.addEventListener(
					'wheel',
					(e: WheelEvent) => {
						if (blockScroll) e.preventDefault();

						if (wheelTimeout !== undefined) return;

						const { left } = u.cursor;
						if (left === undefined || left < 0) return;

						const leftPct = left / rect.width;
						const xVal = u.posToVal(left, 'x');
						const oxRange = u.scales.x.max! - u.scales.x.min!;

						const nxRange = e.deltaY < 0 ? oxRange * factor : oxRange / factor;
						if (nxRange < minZoomSeconds) {
							return;
						}
						const nxMin = xVal - leftPct * nxRange;
						const nxMax = nxMin + nxRange;

						const [clampedMin, clampedMax] = clamp(nxRange, nxMin, nxMax, xRange, xMin, xMax);
						u.batch(() => {
							u.setScale('x', { min: clampedMin, max: clampedMax });
						});

						wheelTimeout = window.setTimeout(() => {
							wheelTimeout = undefined;
						}, WHEEL_THROTTLE_MS);
					},
					wheelListenerOpts
				);

				// Touch pinch-to-zoom
				let lastTouchDist = 0;
				let touchCenter = 0;

				over.addEventListener(
					'touchstart',
					(e: TouchEvent) => {
						if (e.touches.length === 2) {
							const t1 = e.touches[0];
							const t2 = e.touches[1];
							lastTouchDist = Math.abs(t2.clientX - t1.clientX);
							touchCenter = (t1.clientX + t2.clientX) / 2 - rect.left;
						}
					},
					touchListenerOpts
				);

				over.addEventListener(
					'touchmove',
					(e: TouchEvent) => {
						if (e.touches.length === 2) {
							const t1 = e.touches[0];
							const t2 = e.touches[1];
							const dist = Math.abs(t2.clientX - t1.clientX);

							if (lastTouchDist > 0) {
								const scale = dist / lastTouchDist;
								const xVal = u.posToVal(touchCenter, 'x');
								const oxRange = u.scales.x.max! - u.scales.x.min!;
								const leftPct = touchCenter / rect.width;

								const nxRange = oxRange / scale;
								if (nxRange < minZoomSeconds) {
									return;
								}
								const nxMin = xVal - leftPct * nxRange;
								const nxMax = nxMin + nxRange;

								const [clampedMin, clampedMax] = clamp(nxRange, nxMin, nxMax, xRange, xMin, xMax);
								u.batch(() => {
									u.setScale('x', { min: clampedMin, max: clampedMax });
								});
							}

							lastTouchDist = dist;
							touchCenter = (t1.clientX + t2.clientX) / 2 - rect.left;
						}
					},
					touchListenerOpts
				);

				over.addEventListener('touchend', () => {
					lastTouchDist = 0;
				});
			}
		}
	};
}
