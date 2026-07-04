import { describe, expect, it } from 'vitest';
import { parseCsiFrame, parseCsiFrames, type CsiAmplitudeBuffers } from './parseCsiFrame';

function createBuffers(): CsiAmplitudeBuffers {
	return {
		bufferA: new Float32Array(0),
		bufferB: new Float32Array(0),
		flip: false
	};
}

function createFrame({
	timestamp = 1234,
	rssi = -42,
	gainTimesTen = 15,
	motionScore = 2.5,
	isMotionDetected = true,
	iq = [3, 4, 5, 12]
}: {
	timestamp?: number;
	rssi?: number;
	gainTimesTen?: number;
	motionScore?: number;
	isMotionDetected?: boolean;
	iq?: number[];
}) {
	const buffer = new ArrayBuffer(13 + iq.length);
	const view = new DataView(buffer);
	let offset = 0;

	view.setUint32(offset, timestamp, true);
	offset += 4;
	view.setInt8(offset, rssi);
	offset += 1;
	view.setUint16(offset, iq.length, true);
	offset += 2;
	view.setUint8(offset, gainTimesTen);
	offset += 1;
	view.setFloat32(offset, motionScore, true);
	offset += 4;
	view.setUint8(offset, isMotionDetected ? 1 : 0);
	offset += 1;

	for (let index = 0; index < iq.length; index++) {
		view.setInt8(offset + index, iq[index]);
	}

	return buffer;
}

function concatFrames(...frames: ArrayBuffer[]) {
	const totalLength = frames.reduce((sum, frame) => sum + frame.byteLength, 0);
	const batch = new Uint8Array(totalLength);
	let offset = 0;

	for (const frame of frames) {
		batch.set(new Uint8Array(frame), offset);
		offset += frame.byteLength;
	}

	return batch.buffer;
}

describe('parseCsiFrame', () => {
	it('parses metadata and amplitudes from a valid frame', () => {
		const parsed = parseCsiFrame(createFrame({}), createBuffers());

		expect(parsed).not.toBeNull();
		expect(parsed).toMatchObject({
			timestamp: 1234,
			rssi: -42,
			gain: 1.5,
			subcarriers: 2,
			motionScore: 2.5,
			isMotionDetected: true
		});
		expect(Array.from(parsed!.amplitudes)).toEqual([5, 13]);
		expect(parsed!.buffers.flip).toBe(true);
	});

	it('rejects malformed odd-length IQ payloads', () => {
		const parsed = parseCsiFrame(createFrame({ iq: [1, 2, 3] }), createBuffers());
		expect(parsed).toBeNull();
	});

	it('reuses or resizes buffers based on subcarrier count', () => {
		const first = parseCsiFrame(createFrame({ iq: [1, 2, 3, 4] }), createBuffers());
		expect(first).not.toBeNull();

		const second = parseCsiFrame(createFrame({ iq: [2, 0, 0, 2] }), first!.buffers);
		expect(second).not.toBeNull();
		expect(second!.buffers.bufferA.length).toBe(2);
		expect(second!.buffers.bufferB.length).toBe(2);
	});

	it('parses concatenated packet batches', () => {
		const parsed = parseCsiFrames(
			concatFrames(
				createFrame({ timestamp: 1, iq: [3, 4] }),
				createFrame({ timestamp: 2, rssi: -50, iq: [5, 12] })
			),
			createBuffers()
		);

		expect(parsed).not.toBeNull();
		expect(parsed).toHaveLength(2);
		expect(parsed![0].timestamp).toBe(1);
		expect(Array.from(parsed![0].amplitudes)).toEqual([5]);
		expect(parsed![1].timestamp).toBe(2);
		expect(parsed![1].rssi).toBe(-50);
		expect(Array.from(parsed![1].amplitudes)).toEqual([13]);
		expect(parsed![1].buffers.flip).toBe(false);
	});

	it('rejects malformed trailing batch records', () => {
		const malformed = concatFrames(createFrame({ iq: [3, 4] }), new ArrayBuffer(4));
		expect(parseCsiFrames(malformed, createBuffers())).toBeNull();
	});
});
