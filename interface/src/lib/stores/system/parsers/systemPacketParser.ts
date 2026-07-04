import type { SystemStatus } from '$lib/types/system/systemStatus';

export function parseSystemPacket(
	buffer: ArrayBuffer,
	update: (updater: (s: SystemStatus) => SystemStatus) => void
) {
	if (buffer.byteLength < 10) return;
	const view = new DataView(buffer);

	if (view.getUint8(0) !== 0xa5) return;

	let offset = 1;
	const ts = view.getUint32(offset, true);
	offset += 4;

	const wifiStatus = view.getUint8(offset++);
	const wifiFlags = view.getUint8(offset++);
	const rssi = view.getInt8(offset++);

	const now = Date.now();

	let coreTemp = 0;
	if (view.byteLength >= offset + 2) {
		coreTemp = view.getInt16(offset, true) / 10.0;
		offset += 2;
	}

	const isStaConnected = (wifiFlags & 0x01) !== 0;
	const isApMode = (wifiFlags & 0x02) !== 0;
	const isConnected = isStaConnected || isApMode;

	update((_s) => ({
		timestamp: ts * 1000,
		lastUpdate: now,
		wifiStatus,
		rssi,
		isConnected,
		isStaConnected,
		isApMode,
		coreTemp
	}));

	if (typeof window !== 'undefined') {
		import('../../telemetry.svelte').then(({ telemetry }) => {
			telemetry.setRSSI({ rssi, ssid: '' });
		});
	}
}
