const marksSeen = new Set<string>();

function canUsePerformanceMarks(): boolean {
	return (
		typeof performance !== 'undefined' &&
		typeof performance.mark === 'function' &&
		typeof performance.measure === 'function'
	);
}

export function markAppPerformance(name: string, options: { once?: boolean } = {}): boolean {
	if (!canUsePerformanceMarks()) return false;
	if (options.once && marksSeen.has(name)) return false;

	try {
		performance.mark(name);
		marksSeen.add(name);
		return true;
	} catch {
		return false;
	}
}

export function measureAppPerformance(name: string, startMark: string, endMark: string): boolean {
	if (!canUsePerformanceMarks()) return false;

	try {
		performance.measure(name, startMark, endMark);
		return true;
	} catch {
		return false;
	}
}

export function resetAppPerformanceMarksForTests() {
	marksSeen.clear();
}
