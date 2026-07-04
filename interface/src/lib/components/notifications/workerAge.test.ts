import { describe, expect, it } from 'vitest';
import { calculateWorkerAgeSec } from './workerAge';

describe('calculateWorkerAgeSec', () => {
	it('returns -1 when there is no activity timestamp', () => {
		expect(calculateWorkerAgeSec(10_000, 0)).toBe(-1);
	});

	it('returns normal age for timestamps from the same uptime session', () => {
		expect(calculateWorkerAgeSec(12_345, 10_100)).toBe(2);
	});

	it('returns -1 for backwards time that is not a real millis rollover', () => {
		expect(calculateWorkerAgeSec(1_000, 15_000)).toBe(-1);
		expect(calculateWorkerAgeSec(120_000, 180_000)).toBe(-1);
	});

	it('handles real 32-bit millis rollover near the wrap boundary', () => {
		const lastMs = 0xfffffff0;
		const currentUptimeMs = 5_000;

		expect(calculateWorkerAgeSec(currentUptimeMs, lastMs)).toBe(5);
	});
});
