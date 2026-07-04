const UINT32_WRAP_MS = 0x100000000;
const ROLLOVER_WINDOW_MS = 24 * 60 * 60 * 1000;

export function calculateWorkerAgeSec(currentUptimeMs: number, lastMs: number): number {
	if (lastMs === 0) return -1;

	if (currentUptimeMs >= lastMs) {
		return Math.floor((currentUptimeMs - lastMs) / 1000);
	}

	// Only treat backwards time as a real 32-bit millis rollover when both values
	// are close to the wrap boundary. Otherwise this is stale/cross-session data.
	const nearWrap = lastMs >= UINT32_WRAP_MS - ROLLOVER_WINDOW_MS;
	const shortlyAfterWrap = currentUptimeMs <= ROLLOVER_WINDOW_MS;

	if (nearWrap && shortlyAfterWrap) {
		return Math.floor((currentUptimeMs + (UINT32_WRAP_MS - lastMs)) / 1000);
	}

	return -1;
}
