/**
 * Input component variants and utilities
 */

const inputVariants = {
	base: 'input input-bordered input-sm w-full',
	constrained: 'input input-bordered input-sm w-full max-w-xs',
	error: 'input input-bordered input-sm w-full border-error border-2',
	constrainedError: 'input input-bordered input-sm w-full max-w-xs border-error border-2'
} as const;

/**
 * Apply size modifier to input classes
 */
function applyInputSize(classes: string, size: 'xs' | 'sm' | 'md' | 'lg'): string {
	if (!classes.includes('input-')) {
		return `${classes} input-${size}`.trim();
	}

	return classes.replace(/input-(xs|sm|md|lg)/, `input-${size}`);
}

/**
 * Get input classes based on state
 */
export function getInputClasses(
	hasError = false,
	constrained = false,
	size: 'xs' | 'sm' | 'md' | 'lg' = 'md'
): string {
	if (hasError && constrained) return applyInputSize(inputVariants.constrainedError, size);
	if (hasError) return applyInputSize(inputVariants.error, size);
	if (constrained) return applyInputSize(inputVariants.constrained, size);
	return applyInputSize(inputVariants.base, size);
}
