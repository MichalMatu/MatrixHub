import { afterEach, describe, expect, it, vi } from 'vitest';
import {
	markAppPerformance,
	measureAppPerformance,
	resetAppPerformanceMarksForTests
} from './performanceMarks';

describe('performanceMarks', () => {
	afterEach(() => {
		resetAppPerformanceMarksForTests();
		vi.restoreAllMocks();
	});

	it('records marks once when requested', () => {
		const mark = vi.spyOn(performance, 'mark').mockImplementation(
			(name) =>
				({
					name,
					entryType: 'mark',
					startTime: 0,
					duration: 0,
					toJSON: () => ({})
				}) as PerformanceMark
		);

		expect(markAppPerformance('matrixhub:test', { once: true })).toBe(true);
		expect(markAppPerformance('matrixhub:test', { once: true })).toBe(false);
		expect(mark).toHaveBeenCalledOnce();
	});

	it('returns false when a measure cannot be created', () => {
		vi.spyOn(performance, 'measure').mockImplementation(() => {
			throw new Error('missing mark');
		});

		expect(measureAppPerformance('matrixhub:test:measure', 'missing:start', 'missing:end')).toBe(
			false
		);
	});
});
