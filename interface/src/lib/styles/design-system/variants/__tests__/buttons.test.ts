import { describe, expect, it } from 'vitest';

import { buttonVariants, getButtonClasses } from '../buttons';

describe('button variants', () => {
	it('exposes static variant map', () => {
		expect(buttonVariants.primary).toContain('btn-primary');
		expect(buttonVariants.icon.md).toContain('btn-circle');
	});

	it('computes button classes for all variants and sizes', () => {
		expect(getButtonClasses()).toBe(buttonVariants.primary);
		expect(getButtonClasses('secondary', 'lg')).toContain('btn-lg');
		expect(getButtonClasses('ghost', 'sm')).toContain('btn-sm');
		expect(getButtonClasses('danger', 'xs')).toContain('btn-xs');
		expect(getButtonClasses('success')).toContain('btn-success');
		expect(getButtonClasses('warning')).toContain('btn-warning');
		expect(getButtonClasses('accent')).toContain('btn-accent');
		expect(getButtonClasses('neutral')).toContain('btn-neutral');
		expect(getButtonClasses('icon', 'lg')).toBe(buttonVariants.icon.lg);
		// eslint-disable-next-line @typescript-eslint/no-explicit-any
		expect(getButtonClasses('unknown' as any)).toBe(buttonVariants.primary);
	});
});
