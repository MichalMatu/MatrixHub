import { beforeEach, describe, expect, it, vi } from 'vitest';

const { loggerError } = vi.hoisted(() => ({
	loggerError: vi.fn()
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: loggerError
	}
}));

describe('timeFormat', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('formats long date time without converting the encoded wall time', async () => {
		const { formatLongDateTime } = await import('./timeFormat');

		expect(formatLongDateTime('2026-04-12T10:20:30+02:00')).toBe(
			'12 April 2026 at 10:20:30 UTC+02:00'
		);
	});

	it('prefers an explicit timezone label over the raw offset', async () => {
		const { formatLongDateTime } = await import('./timeFormat');

		expect(formatLongDateTime('2026-04-12T10:20:30+02:00', 'CEST')).toBe(
			'12 April 2026 at 10:20:30 CEST'
		);
	});

	it('formats UTC date time with a real UTC conversion', async () => {
		const { formatUTCDateTime } = await import('./timeFormat');

		expect(formatUTCDateTime('2026-04-12T10:20:30+02:00')).toBe('12 April 2026 at 08:20:30 UTC');
	});

	it('returns Invalid date and logs formatting failures', async () => {
		const { formatLongDateTime } = await import('./timeFormat');

		expect(formatLongDateTime('not-a-date')).toBe('Invalid date');
		expect(loggerError).toHaveBeenCalledWith(
			'Error formatting date:',
			'not-a-date',
			expect.any(Error)
		);
	});
});
