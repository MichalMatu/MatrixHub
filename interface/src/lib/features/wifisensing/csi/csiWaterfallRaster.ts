export const CSI_WATERFALL_HISTORY = 200;
const MAX_AMPLITUDE = 120;
const PALETTE_SIZE = 256;

function hslToRgb(hueDegrees: number, saturation: number, lightness: number) {
	const chroma = (1 - Math.abs(2 * lightness - 1)) * saturation;
	const huePrime = hueDegrees / 60;
	const x = chroma * (1 - Math.abs((huePrime % 2) - 1));

	let red = 0;
	let green = 0;
	let blue = 0;

	if (huePrime >= 0 && huePrime < 1) {
		red = chroma;
		green = x;
	} else if (huePrime < 2) {
		red = x;
		green = chroma;
	} else if (huePrime < 3) {
		green = chroma;
		blue = x;
	} else if (huePrime < 4) {
		green = x;
		blue = chroma;
	} else if (huePrime < 5) {
		red = x;
		blue = chroma;
	} else {
		red = chroma;
		blue = x;
	}

	const match = lightness - chroma / 2;

	return [
		Math.round((red + match) * 255),
		Math.round((green + match) * 255),
		Math.round((blue + match) * 255)
	] as const;
}

export function createCsiWaterfallPalette() {
	const palette = new Uint8ClampedArray(PALETTE_SIZE * 4);

	for (let index = 0; index < PALETTE_SIZE; index++) {
		const normalized = index / (PALETTE_SIZE - 1);
		const hue = (1 - normalized) * 240;
		const [red, green, blue] = hslToRgb(hue, 1, 0.5);
		const offset = index * 4;

		palette[offset] = red;
		palette[offset + 1] = green;
		palette[offset + 2] = blue;
		palette[offset + 3] = 255;
	}

	return palette;
}

function getPaletteIndex(amplitude: number) {
	const normalized = Math.min(Math.max(amplitude / MAX_AMPLITUDE, 0), 1);
	const gammaCorrected = Math.pow(normalized, 0.8);
	return Math.round(gammaCorrected * (PALETTE_SIZE - 1));
}

export function shiftWaterfallRows(pixels: Uint8ClampedArray, width: number, height: number) {
	const rowStride = width * 4;
	if (rowStride === 0 || height <= 1) return;

	pixels.copyWithin(rowStride, 0, rowStride * (height - 1));
}

export function writeWaterfallRow(
	targetRow: Uint8ClampedArray,
	amplitudes: Float32Array,
	palette: Uint8ClampedArray
) {
	for (let index = 0; index < amplitudes.length; index++) {
		const paletteOffset = getPaletteIndex(amplitudes[index]) * 4;
		const targetOffset = index * 4;

		targetRow[targetOffset] = palette[paletteOffset];
		targetRow[targetOffset + 1] = palette[paletteOffset + 1];
		targetRow[targetOffset + 2] = palette[paletteOffset + 2];
		targetRow[targetOffset + 3] = palette[paletteOffset + 3];
	}
}

export function updateWaterfallPixels(
	pixels: Uint8ClampedArray,
	width: number,
	height: number,
	amplitudes: Float32Array,
	palette: Uint8ClampedArray
) {
	if (width <= 0 || height <= 0 || amplitudes.length !== width) return;

	shiftWaterfallRows(pixels, width, height);
	// The first row is fully overwritten, so we can write directly into the shared image buffer.
	writeWaterfallRow(pixels.subarray(0, width * 4), amplitudes, palette);
}

export function formatCsiTimestamp(us: number) {
	const totalSeconds = Math.floor(us / 1_000_000);
	const hours = Math.floor(totalSeconds / 3600);
	const minutes = Math.floor((totalSeconds % 3600) / 60);
	const seconds = totalSeconds % 60;

	return `${hours.toString().padStart(2, '0')}h ${minutes.toString().padStart(2, '0')}m ${seconds
		.toString()
		.padStart(2, '0')}s`;
}
