import { normalizeMac } from '$lib/utils/ble';
import type { SystemEventBusLike } from './packetTypes';

export function parseAirMouseBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	if (buffer.byteLength < 33) return;

	const view = new DataView(buffer);
	systemEvents.set({
		type: 'airmouse',
		data: {
			gx: view.getFloat32(5, true),
			gy: view.getFloat32(9, true),
			gz: view.getFloat32(13, true),
			ax: view.getFloat32(17, true),
			ay: view.getFloat32(21, true),
			az: view.getFloat32(25, true),
			deltaG: view.getFloat32(29, true)
		}
	});
}

export function parseShellyBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	const view = new DataView(buffer);
	let offset = 1;

	const idLen = view.getUint8(offset++);
	if (buffer.byteLength < offset + idLen + 13) return;

	const idBytes = new Uint8Array(buffer, offset, idLen);
	const id = new TextDecoder().decode(idBytes);
	offset += idLen;

	const flags = view.getUint8(offset++);
	const online = (flags & 0x01) !== 0;
	const on = (flags & 0x02) !== 0;

	const power = view.getUint16(offset, true) / 100;
	offset += 2;

	const voltage = view.getUint16(offset, true) / 10;
	offset += 2;

	const current = view.getUint16(offset, true) / 1000;
	offset += 2;

	const energy = view.getUint32(offset, true) / 10;
	offset += 4;

	const temperature = view.getInt8(offset++);
	const rssi = view.getInt8(offset++);

	systemEvents.set({
		type: 'shelly',
		data: { id, online, on, power, voltage, current, energy, temperature, rssi }
	});
}

export function parseSensingBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	if (buffer.byteLength < 11) return;

	const view = new DataView(buffer);
	const timestamp = view.getUint32(1, true);
	const rssi = view.getInt8(5);
	const variance = view.getFloat32(6, true);
	const motion = view.getUint8(10) === 1;

	systemEvents.set({
		type: 'sensing',
		data: { timestamp, rssi, variance, motion }
	});
}

export function parseBleBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	if (buffer.byteLength < 13) return;
	const view = new DataView(buffer);

	const macBytes = new Uint8Array(buffer.slice(1, 7));
	const mac = normalizeMac(
		Array.from(macBytes)
			.map((b) => b.toString(16).padStart(2, '0'))
			.join(':')
	);

	const temp = view.getInt16(7, true) / 10.0;
	const humid = view.getUint16(9, true) / 10.0;
	const batt = view.getUint8(11);
	const rssi = view.getInt8(12);

	systemEvents.set({
		type: 'ble',
		data: {
			mac,
			temp,
			humid,
			batt,
			rssi,
			lastSeen: Date.now()
		}
	});
}

export function parseTelemetryBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	if (buffer.byteLength < 11) return;
	const view = new DataView(buffer);

	const co2 = view.getUint16(1, true);
	const temp = view.getInt16(3, true) / 10.0;
	const humid = view.getUint16(5, true) / 10.0;
	const timestamp = view.getUint32(7, true);

	systemEvents.set({
		type: 'sensor',
		data: {
			co2,
			temp,
			humid,
			timestamp_ms: timestamp,
			// Newer firmware appends one status byte (bit0=lastReadOk). Older
			// 11-byte packets are still treated as successful sensor updates.
			lastReadOk: buffer.byteLength >= 12 ? (view.getUint8(11) & 0x01) !== 0 : true
		}
	});
}

export function parseAlarmBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	const ruleIdBytes = 32;
	const headerSize = 1;
	const payloadOffset = headerSize + ruleIdBytes;
	const minPacketSize = payloadOffset + 1 + 1 + 4;

	if (buffer.byteLength < minPacketSize) return;
	const view = new DataView(buffer);
	const idBytes = new Uint8Array(buffer, headerSize, ruleIdBytes);
	let idLength = idBytes.indexOf(0);
	if (idLength === -1) {
		idLength = idBytes.length;
	}

	const id = new TextDecoder().decode(idBytes.subarray(0, idLength));
	const triggered = view.getUint8(payloadOffset) === 1;
	const severity = view.getUint8(payloadOffset + 1);
	const currentValue = view.getFloat32(payloadOffset + 2, true);

	systemEvents.set({
		type: 'alarm',
		data: {
			id,
			triggered,
			current_value: currentValue,
			severity
		}
	});
}

export function parseTelegramBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	if (buffer.byteLength < 18) return;
	const view = new DataView(buffer);

	const flags = view.getUint8(1);
	const enabled = (flags & 0x01) !== 0;
	const running = (flags & 0x02) !== 0;
	const lastPollAgeSec = view.getUint16(2, true);
	const messagesProcessed = view.getUint32(4, true);
	const messagesSent = view.getUint32(8, true);
	const commandsExecuted = view.getUint32(12, true);
	const lastHttpCode = view.getInt16(16, true);

	systemEvents.set({
		type: 'telegram',
		data: {
			enabled,
			running,
			lastPollAgeSec,
			messagesProcessed,
			messagesSent,
			commandsExecuted,
			lastHttpCode
		}
	});
}

export function parseNotifStatsBinary(buffer: ArrayBuffer, systemEvents: SystemEventBusLike) {
	if (buffer.byteLength < 112) return;
	const view = new DataView(buffer);
	let offset = 1;

	const webhook = {
		sent: view.getUint32(offset, true),
		failed: view.getUint32(offset + 4, true),
		lastMs: view.getUint32(offset + 8, true),
		httpCode: view.getInt16(offset + 12, true)
	};
	offset += 14;

	const pushover = {
		sent: view.getUint32(offset, true),
		failed: view.getUint32(offset + 4, true),
		lastMs: view.getUint32(offset + 8, true),
		httpCode: view.getInt16(offset + 12, true)
	};
	offset += 14;

	const udp = {
		sent: view.getUint32(offset, true),
		failed: view.getUint32(offset + 4, true),
		lastMs: view.getUint32(offset + 8, true)
	};
	offset += 12;

	const heartbeat = [];
	for (let i = 0; i < 4; i++) {
		heartbeat.push({
			lastPingMs: view.getUint32(offset, true),
			successCount: view.getUint32(offset + 4, true),
			failCount: view.getUint32(offset + 8, true)
		});
		offset += 12;
	}

	const telegramFlags = view.getUint8(offset);
	const telegram = {
		enabled: (telegramFlags & 0x01) !== 0,
		running: (telegramFlags & 0x02) !== 0,
		lastActivityMs: view.getUint32(offset + 1, true),
		messagesProcessed: view.getUint32(offset + 5, true),
		messagesSent: view.getUint32(offset + 9, true),
		commandsExecuted: view.getUint32(offset + 13, true),
		lastHttpCode: view.getInt16(offset + 17, true)
	};
	offset += 19;

	const uptimeMs = view.getUint32(offset, true);

	systemEvents.set({
		type: 'notif_stats',
		data: { webhook, pushover, udp, heartbeat, telegram, uptimeMs }
	});
}
