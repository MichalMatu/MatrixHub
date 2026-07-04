import { describe, expect, it } from 'vitest';
import { basename, getBreadcrumbs, parentPath } from './helpers';

describe('path helpers', () => {
	it('returns the basename while ignoring trailing slashes', () => {
		expect(basename('/logs/2026/04/')).toBe('04');
		expect(basename('/')).toBe('/');
	});

	it('returns the parent path for nested and root paths', () => {
		expect(parentPath('/logs/2026/04')).toBe('/logs/2026');
		expect(parentPath('/logs')).toBe('/');
		expect(parentPath('')).toBe('/');
	});

	it('builds breadcrumbs with normalized paths', () => {
		expect(getBreadcrumbs('/logs//2026/04/')).toEqual([
			{ label: 'logs', path: '/logs' },
			{ label: '2026', path: '/logs/2026' },
			{ label: '04', path: '/logs/2026/04' }
		]);
	});

	it('returns no breadcrumbs for root-like paths', () => {
		expect(getBreadcrumbs('/')).toEqual([]);
		expect(getBreadcrumbs('')).toEqual([]);
	});
});
