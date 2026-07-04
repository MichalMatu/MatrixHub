import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';

export function parseMacroBinary(buffer: ArrayBuffer, updateMacros: (m: ScriptStatus) => void) {
	if (buffer.byteLength < 11) return;
	const view = new DataView(buffer);

	const statusId = view.getUint8(1);
	const current_line = view.getUint32(2, true);
	const uptime_ms = view.getUint32(6, true);

	const statusMap = ['IDLE', 'RUNNING', 'PAUSED', 'ERROR', 'COMPLETED'];
	const status = statusMap[statusId] || 'IDLE';

	const bytes = new Uint8Array(buffer);
	let end = 10;
	while (end < bytes.length && bytes[end] !== 0) {
		end++;
	}

	const decoder = new TextDecoder();
	const scriptBytes = new Uint8Array(buffer, 10, end - 10);
	const current_script = decoder.decode(scriptBytes);

	let last_error = '';
	if (end + 1 < bytes.length) {
		let errEnd = end + 1;
		while (errEnd < bytes.length && bytes[errEnd] !== 0) {
			errEnd++;
		}
		const errBytes = new Uint8Array(buffer, end + 1, errEnd - (end + 1));
		last_error = decoder.decode(errBytes);
	}

	updateMacros({
		status: status as 'IDLE' | 'RUNNING' | 'PAUSED' | 'ERROR' | 'COMPLETED',
		current_script,
		current_line,
		uptime_ms,
		last_error
	});
}
