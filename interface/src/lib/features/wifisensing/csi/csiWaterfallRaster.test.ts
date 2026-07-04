import { describe, expect, it } from 'vitest';
import {
	formatCsiTimestamp,
	shiftWaterfallRows,
	updateWaterfallPixels,
	writeWaterfallRow
} from './csiWaterfallRaster';

function createTestPalette() {
	const palette = new Uint8ClampedArray(256 * 4);

	for (let index = 0; index < 256; index++) {
		const offset = index * 4;
		palette[offset] = index;
		palette[offset + 3] = 255;
	}

	return palette;
}

describe('csiWaterfallRaster', () => {
	it('formats CSI timestamps in hh mm ss', () => {
		expect(formatCsiTimestamp(3_661_000_000)).toBe('01h 01m 01s');
	});

	it('writes the top waterfall row from amplitudes', () => {
		const row = new Uint8ClampedArray(8);
		writeWaterfallRow(row, new Float32Array([0, 120]), createTestPalette());

		expect(Array.from(row)).toEqual([0, 0, 0, 255, 255, 0, 0, 255]);
	});

	it('shifts existing rows down before drawing the latest frame', () => {
		const pixels = new Uint8ClampedArray(2 * 3 * 4);
		const palette = createTestPalette();

		updateWaterfallPixels(pixels, 2, 3, new Float32Array([0, 120]), palette);
		updateWaterfallPixels(pixels, 2, 3, new Float32Array([120, 0]), palette);

		expect(Array.from(pixels.slice(0, 8))).toEqual([255, 0, 0, 255, 0, 0, 0, 255]);
		expect(Array.from(pixels.slice(8, 16))).toEqual([0, 0, 0, 255, 255, 0, 0, 255]);
	});

	it('can shift rows in-place without allocating', () => {
		const pixels = new Uint8ClampedArray([
			1, 0, 0, 255, 2, 0, 0, 255, 3, 0, 0, 255, 4, 0, 0, 255, 5, 0, 0, 255, 6, 0, 0, 255
		]);

		shiftWaterfallRows(pixels, 2, 3);

		expect(Array.from(pixels)).toEqual([
			1, 0, 0, 255, 2, 0, 0, 255, 1, 0, 0, 255, 2, 0, 0, 255, 3, 0, 0, 255, 4, 0, 0, 255
		]);
	});
});
