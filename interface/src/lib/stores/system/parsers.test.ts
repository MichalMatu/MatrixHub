import { describe, it, expect, vi } from 'vitest';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';
import type { SystemStatus } from '$lib/types/system/systemStatus';
import { SystemEventBus } from '../systemStatus.svelte';
import { parseBinaryPacket } from './parsers';

function captureSystemEvent(buffer: ArrayBuffer) {
	let captured: unknown = null;
	parseBinaryPacket(buffer, {
		systemEvents: {
			set: (value) => {
				captured = value;
			}
		},
		updateStatus: () => {}
	});
	return captured;
}

describe('parseBinaryPacket', () => {
	it('parses current_script and last_error from macro packet', () => {
		const script = 'foo.txt';
		const error = 'boom';
		const encoder = new TextEncoder();
		const scriptBytes = encoder.encode(script);
		const errorBytes = encoder.encode(error);

		const buf = new Uint8Array(10 + scriptBytes.length + 1 + errorBytes.length + 1);
		let off = 0;
		buf[off++] = 0x4d; // 'M'
		buf[off++] = 3; // ERROR
		// current_line = 7
		buf[off++] = 7;
		buf[off++] = 0;
		buf[off++] = 0;
		buf[off++] = 0;
		// uptime_ms = 1234
		const uptime = 1234;
		buf[off++] = uptime & 0xff;
		buf[off++] = (uptime >> 8) & 0xff;
		buf[off++] = (uptime >> 16) & 0xff;
		buf[off++] = (uptime >> 24) & 0xff;
		buf.set(scriptBytes, off);
		off += scriptBytes.length;
		buf[off++] = 0;
		buf.set(errorBytes, off);
		off += errorBytes.length;
		buf[off++] = 0;

		let captured!: ScriptStatus;
		const systemEvents = new SystemEventBus();
		parseBinaryPacket(buf.buffer, {
			systemEvents,
			updateStatus: () => {},
			updateMacros: (m: ScriptStatus) => {
				captured = m;
			}
		});

		expect(captured.current_script).toBe('foo.txt');
		expect(captured.last_error).toBe('boom');
		expect(captured.status).toBe('ERROR');
		expect(captured.current_line).toBe(7);
	});

	it('parses scanner-only system packets with wifi flag byte', async () => {
		const updateStatus = vi.fn((updater: (state: SystemStatus) => SystemStatus) =>
			updater({} as never)
		);
		const buf = new Uint8Array(10);
		const view = new DataView(buf.buffer);
		buf[0] = 0xa5;
		view.setUint32(1, 123, true);
		buf[5] = 3;
		buf[6] = 0x01;
		view.setInt8(7, -61);
		view.setInt16(8, 287, true);

		parseBinaryPacket(buf.buffer, {
			systemEvents: new SystemEventBus(),
			updateStatus
		});

		expect(updateStatus).toHaveBeenCalledTimes(1);
		const nextState = updateStatus.mock.calls[0]?.[0]?.({} as never);
		expect(nextState).toMatchObject({
			timestamp: 123000,
			wifiStatus: 3,
			rssi: -61,
			isConnected: true,
			isStaConnected: true,
			isApMode: false,
			coreTemp: 28.7
		});

		await Promise.resolve();
	});

	it('drops unknown packets without mutating stores', () => {
		const updateStatus = vi.fn();
		const systemEvents = { set: vi.fn() };

		parseBinaryPacket(new Uint8Array([0x11, 0x22, 0x33]).buffer, {
			systemEvents,
			updateStatus
		});

		expect(systemEvents.set).not.toHaveBeenCalled();
		expect(updateStatus).not.toHaveBeenCalled();
	});

	it('parses alarm packets using stable rule id', () => {
		const alarmId = 'alarm-7';
		const encoder = new TextEncoder();
		const alarmIdBytes = encoder.encode(alarmId);
		const buf = new Uint8Array(39);
		const view = new DataView(buf.buffer);
		buf[0] = 0x41; // 'A'
		buf.set(alarmIdBytes, 1);
		buf[33] = 1;
		buf[34] = 2;
		view.setFloat32(35, 42.5, true);

		let captured: unknown = null;
		parseBinaryPacket(buf.buffer, {
			systemEvents: {
				set: (value) => {
					captured = value;
				}
			},
			updateStatus: () => {}
		});

		expect(captured).toEqual({
			type: 'alarm',
			data: {
				id: 'alarm-7',
				triggered: true,
				current_value: 42.5,
				severity: 2
			}
		});
	});

	it('parses Shelly packets', () => {
		const encoder = new TextEncoder();
		const id = encoder.encode('plug-1');
		const buf = new Uint8Array(1 + 1 + id.length + 1 + 2 + 2 + 2 + 4 + 1 + 1);
		const view = new DataView(buf.buffer);
		let offset = 0;
		buf[offset++] = 0x53;
		buf[offset++] = id.length;
		buf.set(id, offset);
		offset += id.length;
		buf[offset++] = 0x03;
		view.setUint16(offset, 1234, true);
		offset += 2;
		view.setUint16(offset, 2305, true);
		offset += 2;
		view.setUint16(offset, 456, true);
		offset += 2;
		view.setUint32(offset, 789, true);
		offset += 4;
		view.setInt8(offset++, 26);
		view.setInt8(offset, -55);

		expect(captureSystemEvent(buf.buffer)).toEqual({
			type: 'shelly',
			data: {
				id: 'plug-1',
				online: true,
				on: true,
				power: 12.34,
				voltage: 230.5,
				current: 0.456,
				energy: 78.9,
				temperature: 26,
				rssi: -55
			}
		});
	});

	it('parses WiFi sensing packets', () => {
		const buf = new Uint8Array(11);
		const view = new DataView(buf.buffer);
		buf[0] = 0x57;
		view.setUint32(1, 2222, true);
		view.setInt8(5, -70);
		view.setFloat32(6, 0.75, true);
		buf[10] = 1;

		expect(captureSystemEvent(buf.buffer)).toEqual({
			type: 'sensing',
			data: {
				timestamp: 2222,
				rssi: -70,
				variance: 0.75,
				motion: true
			}
		});
	});

	it('parses BLE packets', () => {
		const buf = new Uint8Array(13);
		const view = new DataView(buf.buffer);
		buf[0] = 0x42;
		buf.set([0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff], 1);
		view.setInt16(7, 215, true);
		view.setUint16(9, 503, true);
		buf[11] = 88;
		view.setInt8(12, -48);

		expect(captureSystemEvent(buf.buffer)).toMatchObject({
			type: 'ble',
			data: {
				mac: 'aa:bb:cc:dd:ee:ff',
				temp: 21.5,
				humid: 50.3,
				batt: 88,
				rssi: -48
			}
		});
	});

	it('parses telemetry packets', () => {
		const buf = new Uint8Array(12);
		const view = new DataView(buf.buffer);
		buf[0] = 0x54;
		view.setUint16(1, 612, true);
		view.setInt16(3, 241, true);
		view.setUint16(5, 455, true);
		view.setUint32(7, 9999, true);
		buf[11] = 0x00;

		expect(captureSystemEvent(buf.buffer)).toEqual({
			type: 'sensor',
			data: {
				co2: 612,
				temp: 24.1,
				humid: 45.5,
				timestamp_ms: 9999,
				lastReadOk: false
			}
		});
	});

	it('keeps backward compatibility with legacy 11-byte telemetry packets', () => {
		const buf = new Uint8Array(11);
		const view = new DataView(buf.buffer);
		buf[0] = 0x54;
		view.setUint16(1, 700, true);
		view.setInt16(3, 255, true);
		view.setUint16(5, 500, true);
		view.setUint32(7, 4242, true);

		expect(captureSystemEvent(buf.buffer)).toEqual({
			type: 'sensor',
			data: {
				co2: 700,
				temp: 25.5,
				humid: 50,
				timestamp_ms: 4242,
				lastReadOk: true
			}
		});
	});

	it('parses Telegram runtime packets', () => {
		const buf = new Uint8Array(18);
		const view = new DataView(buf.buffer);
		buf[0] = 0x47;
		buf[1] = 0x03;
		view.setUint16(2, 15, true);
		view.setUint32(4, 6, true);
		view.setUint32(8, 7, true);
		view.setUint32(12, 8, true);
		view.setInt16(16, 200, true);

		expect(captureSystemEvent(buf.buffer)).toEqual({
			type: 'telegram',
			data: {
				enabled: true,
				running: true,
				lastPollAgeSec: 15,
				messagesProcessed: 6,
				messagesSent: 7,
				commandsExecuted: 8,
				lastHttpCode: 200
			}
		});
	});

	it('parses notification stats packets', () => {
		const buf = new Uint8Array(112);
		const view = new DataView(buf.buffer);
		let offset = 0;
		buf[offset++] = 0x4e;

		view.setUint32(offset, 1, true);
		view.setUint32(offset + 4, 2, true);
		view.setUint32(offset + 8, 3, true);
		view.setInt16(offset + 12, 204, true);
		offset += 14;

		view.setUint32(offset, 4, true);
		view.setUint32(offset + 4, 5, true);
		view.setUint32(offset + 8, 6, true);
		view.setInt16(offset + 12, 500, true);
		offset += 14;

		view.setUint32(offset, 7, true);
		view.setUint32(offset + 4, 8, true);
		view.setUint32(offset + 8, 9, true);
		offset += 12;

		for (let i = 0; i < 4; i++) {
			view.setUint32(offset, i + 10, true);
			view.setUint32(offset + 4, i + 20, true);
			view.setUint32(offset + 8, i + 30, true);
			offset += 12;
		}

		buf[offset] = 0x03;
		view.setUint32(offset + 1, 100, true);
		view.setUint32(offset + 5, 101, true);
		view.setUint32(offset + 9, 102, true);
		view.setUint32(offset + 13, 103, true);
		view.setInt16(offset + 17, 201, true);
		offset += 19;

		view.setUint32(offset, 120000, true);

		expect(captureSystemEvent(buf.buffer)).toMatchObject({
			type: 'notif_stats',
			data: {
				webhook: { sent: 1, failed: 2, lastMs: 3, httpCode: 204 },
				pushover: { sent: 4, failed: 5, lastMs: 6, httpCode: 500 },
				udp: { sent: 7, failed: 8, lastMs: 9 },
				telegram: {
					enabled: true,
					running: true,
					lastActivityMs: 100,
					messagesProcessed: 101,
					messagesSent: 102,
					commandsExecuted: 103,
					lastHttpCode: 201
				},
				uptimeMs: 120000
			}
		});
	});

	it('parses AirMouse IMU packets on the shared system websocket', () => {
		const buf = new Uint8Array(33);
		const view = new DataView(buf.buffer);
		buf[0] = 0x49; // 'I'
		view.setUint32(1, 1234, true);
		view.setFloat32(5, 1.25, true);
		view.setFloat32(9, 2.5, true);
		view.setFloat32(13, 3.75, true);
		view.setFloat32(17, 4.0, true);
		view.setFloat32(21, 5.0, true);
		view.setFloat32(25, 6.0, true);
		view.setFloat32(29, 0.9, true);

		let captured: unknown = null;
		parseBinaryPacket(buf.buffer, {
			systemEvents: {
				set: (value) => {
					captured = value;
				}
			},
			updateStatus: () => {}
		});

		expect(captured).toMatchObject({
			type: 'airmouse',
			data: {
				gx: 1.25,
				gy: 2.5,
				gz: 3.75,
				ax: 4.0,
				ay: 5.0,
				az: 6.0
			}
		});
		expect((captured as { data: { deltaG: number } }).data.deltaG).toBeCloseTo(0.9, 5);
	});
});
