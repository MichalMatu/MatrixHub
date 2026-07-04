import { describe, expect, it, beforeEach } from 'vitest';
import {
	MATRIX_LAST_TAB_STORAGE_KEY,
	getRememberedMatrixTabPath,
	isMatrixTabPath,
	rememberMatrixTabPath
} from './matrixNavigation';

describe('matrixNavigation', () => {
	beforeEach(() => {
		localStorage.clear();
	});

	it('remembers only known Matrix tab paths', () => {
		rememberMatrixTabPath('/system/matrix/data');

		expect(localStorage.getItem(MATRIX_LAST_TAB_STORAGE_KEY)).toBe('/system/matrix/data');
		expect(getRememberedMatrixTabPath()).toBe('/system/matrix/data');

		rememberMatrixTabPath('/system/status');

		expect(localStorage.getItem(MATRIX_LAST_TAB_STORAGE_KEY)).toBe('/system/matrix/data');
	});

	it('rejects stale or malformed remembered paths', () => {
		localStorage.setItem(MATRIX_LAST_TAB_STORAGE_KEY, '/system/unknown');

		expect(getRememberedMatrixTabPath()).toBeNull();
		expect(isMatrixTabPath('/system/matrix/effects')).toBe(true);
		expect(isMatrixTabPath('/system/unknown')).toBe(false);
	});
});
