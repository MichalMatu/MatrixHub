import {
	parseAirMouseBinary,
	parseAlarmBinary,
	parseBleBinary,
	parseNotifStatsBinary,
	parseSensingBinary,
	parseShellyBinary,
	parseTelegramBinary,
	parseTelemetryBinary
} from './parsers/eventPacketParsers';
import { parseMacroBinary } from './parsers/macroPacketParser';
import type { StoreMappers } from './parsers/packetTypes';
import { parseSystemPacket } from './parsers/systemPacketParser';

export type { StoreMappers } from './parsers/packetTypes';

export function parseBinaryPacket(buffer: ArrayBuffer, stores: StoreMappers) {
	if (buffer.byteLength < 2) return;
	const magic = new DataView(buffer).getUint8(0);

	switch (magic) {
		case 0x53:
			parseShellyBinary(buffer, stores.systemEvents);
			return;
		case 0x57:
			parseSensingBinary(buffer, stores.systemEvents);
			return;
		case 0x42:
			parseBleBinary(buffer, stores.systemEvents);
			return;
		case 0x54:
			parseTelemetryBinary(buffer, stores.systemEvents);
			return;
		case 0x41:
			parseAlarmBinary(buffer, stores.systemEvents);
			return;
		case 0x47:
			parseTelegramBinary(buffer, stores.systemEvents);
			return;
		case 0x4e:
			parseNotifStatsBinary(buffer, stores.systemEvents);
			return;
		case 0x49:
			parseAirMouseBinary(buffer, stores.systemEvents);
			return;
		case 0x4d:
			if (stores.updateMacros) {
				parseMacroBinary(buffer, stores.updateMacros);
			}
			return;
		default:
			parseSystemPacket(buffer, stores.updateStatus);
	}
}
