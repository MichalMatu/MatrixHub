import { describe, expect, it } from 'vitest';
import { parseBinaryLog, estimateRecordCount, BinaryLogParseError } from './binaryLogParser';

const MAGIC = 0x504c4e54;
const VERSION = 3;
const HEADER_SIZE = 8;
const RECORD_SIZE = 10;

interface TestRecord {
	timestamp: number;
	co2: number;
	temp10x: number;
	humid10x: number;
}

function makeLog(records: TestRecord[], overrides: { magic?: number; version?: number } = {}) {
	const buffer = new ArrayBuffer(HEADER_SIZE + records.length * RECORD_SIZE);
	const view = new DataView(buffer);
	view.setUint32(0, overrides.magic ?? MAGIC, true);
	view.setUint8(4, overrides.version ?? VERSION);
	view.setUint8(5, RECORD_SIZE);
	view.setUint16(6, 0, true);

	records.forEach((record, index) => {
		const offset = HEADER_SIZE + index * RECORD_SIZE;
		view.setUint32(offset, record.timestamp, true);
		view.setUint16(offset + 4, record.co2, true);
		view.setInt16(offset + 6, record.temp10x, true);
		view.setUint16(offset + 8, record.humid10x, true);
	});

	return buffer;
}

describe('binaryLogParser', () => {
	it('parses valid SCD4x records', () => {
		const parsed = parseBinaryLog(
			makeLog([{ timestamp: 1_783_000_000, co2: 650, temp10x: 234, humid10x: 521 }])
		);

		expect(parsed).toEqual({
			timestamps: [1_783_000_000],
			co2s: [650],
			temps: [23.4],
			humids: [52.1]
		});
	});

	it('maps out-of-range sensor values to null without breaking series alignment', () => {
		const parsed = parseBinaryLog(
			makeLog([{ timestamp: 1_783_000_000, co2: 399, temp10x: -201, humid10x: 1001 }])
		);

		expect(parsed.timestamps).toEqual([1_783_000_000]);
		expect(parsed.co2s).toEqual([null]);
		expect(parsed.temps).toEqual([null]);
		expect(parsed.humids).toEqual([null]);
	});

	it('treats the firmware temperature null marker as a null chart point', () => {
		const parsed = parseBinaryLog(
			makeLog([{ timestamp: 1_783_000_000, co2: 500, temp10x: -32768, humid10x: 420 }])
		);

		expect(parsed.temps).toEqual([null]);
		expect(parsed.co2s).toEqual([500]);
		expect(parsed.humids).toEqual([42]);
	});

	it('keeps offline-only timestamps but drops pre-epoch points from mixed logs', () => {
		const offline = parseBinaryLog(
			makeLog([
				{ timestamp: 10, co2: 500, temp10x: 210, humid10x: 420 },
				{ timestamp: 9, co2: 510, temp10x: 211, humid10x: 421 }
			])
		);
		expect(offline.timestamps).toEqual([10, 11]);

		const mixed = parseBinaryLog(
			makeLog([
				{ timestamp: 10, co2: 500, temp10x: 210, humid10x: 420 },
				{ timestamp: 1_783_000_000, co2: 510, temp10x: 211, humid10x: 421 }
			])
		);
		expect(mixed.timestamps).toEqual([1_783_000_000]);
		expect(mixed.co2s).toEqual([510]);
	});

	it('rejects invalid headers and estimates record counts conservatively', () => {
		expect(() => parseBinaryLog(makeLog([], { magic: 0 }))).toThrow(BinaryLogParseError);
		expect(estimateRecordCount(HEADER_SIZE)).toBe(0);
		expect(estimateRecordCount(HEADER_SIZE + RECORD_SIZE * 3 + 4)).toBe(3);
	});
});
