import { browser } from '$app/environment';

export const MATRIX_DISPLAY_TAB_PATH = '/system/matrix';
export const MATRIX_LAST_TAB_STORAGE_KEY = 'matrixhub.matrix.lastTab';

export const MATRIX_TAB_PATHS = [
	MATRIX_DISPLAY_TAB_PATH,
	'/system/matrix/alarms',
	'/system/matrix/effects',
	'/system/matrix/data'
] as const;

export type MatrixTabPath = (typeof MATRIX_TAB_PATHS)[number];

export function isMatrixTabPath(path: string): path is MatrixTabPath {
	return MATRIX_TAB_PATHS.includes(path as MatrixTabPath);
}

export function rememberMatrixTabPath(path: string) {
	if (!browser || !isMatrixTabPath(path)) return;
	localStorage.setItem(MATRIX_LAST_TAB_STORAGE_KEY, path);
}

export function getRememberedMatrixTabPath(): MatrixTabPath | null {
	if (!browser) return null;
	const path = localStorage.getItem(MATRIX_LAST_TAB_STORAGE_KEY);
	return path && isMatrixTabPath(path) ? path : null;
}
