/**
 * Utility functions for class name manipulation
 */

/**
 * Join classes, filtering out falsy values
 */
function joinClasses(...classes: Array<string | false | null | undefined>): string {
	return classes.filter(Boolean).join(' ');
}

/**
 * Apply surface-card class with additional classes
 */
export function surfaceCard(...classes: Array<string | false | null | undefined>): string {
	return joinClasses('surface-card', ...classes);
}

/**
 * Apply surface-panel class with additional classes
 */
export function surfacePanel(...classes: Array<string | false | null | undefined>): string {
	return joinClasses('surface-panel', ...classes);
}

/**
 * Apply detail-tile class with additional classes
 */
export function detailTile(...classes: Array<string | false | null | undefined>): string {
	return joinClasses('detail-tile', ...classes);
}

/**
 * Apply mini-card class with additional classes
 */
export function miniCard(...classes: Array<string | false | null | undefined>): string {
	return joinClasses('mini-card', ...classes);
}
