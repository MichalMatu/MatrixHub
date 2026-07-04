import { Logger } from '$lib/services/core/Logger';

/**
 * Binary log file format parser (v3 - SCD4x without battery)
 *
 * File structure:
 * - Header: 8 bytes (magic, version, recordSize, reserved)
 * - Records: N × 10 bytes each
 *
 * Record layout (10 bytes, little-endian):
 * - timestamp (uint32, 4B): Unix timestamp in seconds
 * - co2 (uint16, 2B): CO2 concentration in ppm
 * - temp_10x (int16, 2B): Temperature × 10 (-200 to +600 = -20.0°C to +60.0°C)
 * - humid_10x (uint16, 2B): Humidity × 10 (0 to 1000 = 0.0% to 100.0%)
 */

const BINARY_MAGIC = 0x504c4e54; // "PLNT" in ASCII
const EXPECTED_VERSION = 3;
const HEADER_SIZE = 8;
const RECORD_SIZE = 10;
const MIN_VALID_EPOCH = 1_000_000_000; // 2001-09-09 (matches firmware MIN_VALID_EPOCH)
const CO2_MIN_PPM = 400;
const CO2_MAX_PPM = 10_000;
const TEMP_MIN_C = -20;
const TEMP_MAX_C = 70;
const HUMID_MIN_PCT = 0;
const HUMID_MAX_PCT = 100;

interface BinaryLogHeader {
	magic: number;
	version: number;
	recordSize: number;
	reserved: number;
}

export interface ParsedLogData {
	timestamps: number[]; // Unix timestamps in seconds
	co2s: (number | null)[];
	temps: (number | null)[];
	humids: (number | null)[];
}

export class BinaryLogParseError extends Error {
	constructor(message: string) {
		super(message);
		this.name = 'BinaryLogParseError';
	}
}

/**
 * Parse binary log file header
 */
function parseHeader(view: DataView): BinaryLogHeader {
	const header: BinaryLogHeader = {
		magic: view.getUint32(0, true), // little-endian
		version: view.getUint8(4),
		recordSize: view.getUint8(5),
		reserved: view.getUint16(6, true)
	};

	if (header.magic !== BINARY_MAGIC) {
		throw new BinaryLogParseError(
			`Invalid magic: 0x${header.magic.toString(16)} (expected 0x${BINARY_MAGIC.toString(16)})`
		);
	}

	if (header.version !== EXPECTED_VERSION) {
		throw new BinaryLogParseError(
			`Unsupported version: ${header.version} (expected ${EXPECTED_VERSION})`
		);
	}

	if (header.recordSize !== RECORD_SIZE) {
		throw new BinaryLogParseError(
			`Invalid record size: ${header.recordSize} (expected ${RECORD_SIZE})`
		);
	}

	return header;
}

/**
 * Parse single binary log record
 */
function parseRecord(view: DataView, offset: number) {
	const timestamp = view.getUint32(offset, true);
	const co2 = view.getUint16(offset + 4, true);
	const temp_10x = view.getInt16(offset + 6, true);
	const humid_10x = view.getUint16(offset + 8, true);

	// Convert fixed-point values to floats
	// INT16_MIN is used as NaN marker
	const normalizedCo2 = co2 >= CO2_MIN_PPM && co2 <= CO2_MAX_PPM ? co2 : null;
	const temp = temp_10x === -32768 ? null : temp_10x / 10.0;
	const normalizedTemp = temp !== null && temp >= TEMP_MIN_C && temp <= TEMP_MAX_C ? temp : null;
	const humid = humid_10x / 10.0;
	const normalizedHumid = humid >= HUMID_MIN_PCT && humid <= HUMID_MAX_PCT ? humid : null;

	return {
		timestamp,
		co2: normalizedCo2,
		temp: normalizedTemp,
		humid: normalizedHumid
	};
}

/**
 * Parse complete binary log file
 * @param arrayBuffer Raw binary data from file
 * @returns Parsed data arrays suitable for charting
 */
export function parseBinaryLog(arrayBuffer: ArrayBuffer): ParsedLogData {
	const view = new DataView(arrayBuffer);

	// Parse and validate header
	if (view.byteLength < HEADER_SIZE) {
		throw new BinaryLogParseError('File too small (no header)');
	}

	parseHeader(view);

	// Parse records
	const recordCount = Math.floor((view.byteLength - HEADER_SIZE) / RECORD_SIZE);
	const data: ParsedLogData = {
		timestamps: [],
		co2s: [],
		temps: [],
		humids: []
	};
	let hasValidEpoch = false;

	for (let i = 0; i < recordCount; i++) {
		const offset = HEADER_SIZE + i * RECORD_SIZE;
		try {
			const record = parseRecord(view, offset);
			// Allow offline mode timestamps (1970) to render on charts while mitigating backwards jumps
			if (data.timestamps.length > 0) {
				const lastTs = data.timestamps[data.timestamps.length - 1];
				// Guard against backwards time jumps larger than 1 second causing uPlot Range Error crashes.
				if (record.timestamp < lastTs) {
					// Time jumped backwards (probably a bad RTC glitch). Overwrite with last monotonic time
					record.timestamp = lastTs + 1;
				}
			}
			if (record.timestamp >= MIN_VALID_EPOCH) {
				hasValidEpoch = true;
			}
			data.timestamps.push(record.timestamp);
			data.co2s.push(record.co2);
			data.temps.push(record.temp);
			data.humids.push(record.humid);
		} catch (err) {
			Logger.warn(`Failed to parse record ${i} at offset ${offset}:`, err);
			// Skip invalid records to prevent chart crashes (RangeError in uPlot)
			continue;
		}
	}

	// If we have at least one valid epoch timestamp, drop invalid (pre-2001) points.
	// This preserves offline mode charts (all timestamps ~1970) while fixing mixed data.
	if (!hasValidEpoch) return data;

	const filtered: ParsedLogData = { timestamps: [], co2s: [], temps: [], humids: [] };
	for (let i = 0; i < data.timestamps.length; i++) {
		if (data.timestamps[i] < MIN_VALID_EPOCH) continue;
		filtered.timestamps.push(data.timestamps[i]);
		filtered.co2s.push(data.co2s[i]);
		filtered.temps.push(data.temps[i]);
		filtered.humids.push(data.humids[i]);
	}
	return filtered;
}

/**
 * Calculate estimated record count from file size
 */
export function estimateRecordCount(fileSize: number): number {
	if (fileSize <= HEADER_SIZE) return 0;
	return Math.floor((fileSize - HEADER_SIZE) / RECORD_SIZE);
}
