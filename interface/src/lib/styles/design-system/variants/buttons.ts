/**
 * Button component variants and utilities
 */

export const buttonVariants = {
	// Size variants
	size: {
		xs: 'btn btn-xs',
		sm: 'btn btn-sm',
		md: 'btn',
		lg: 'btn btn-lg'
	},

	// Action variants
	primary: 'btn btn-primary',
	secondary: 'btn btn-outline',
	ghost: 'btn btn-ghost',
	danger: 'btn btn-error',
	success: 'btn btn-success',
	warning: 'btn btn-warning text-warning-content',
	accent: 'btn btn-accent',
	neutral: 'btn btn-neutral text-neutral-content',

	// Icon buttons
	icon: {
		xs: 'btn btn-ghost btn-circle btn-xs',
		sm: 'btn btn-ghost btn-circle btn-sm',
		md: 'btn btn-ghost btn-circle',
		lg: 'btn btn-ghost btn-circle btn-lg'
	}
} as const;

export type ButtonVariant = keyof Omit<typeof buttonVariants, 'size' | 'icon'> | 'icon';
export type ButtonSize = keyof typeof buttonVariants.size;

/**
 * Get button classes based on variant and size
 */
export function getButtonClasses(
	variant: ButtonVariant = 'primary',
	size: ButtonSize = 'md'
): string {
	// Handle icon buttons separately
	if (variant === 'icon') {
		return buttonVariants.icon[size] || buttonVariants.icon.md;
	}

	// Get base variant class
	let baseClasses: string;
	if (variant === 'primary') baseClasses = buttonVariants.primary;
	else if (variant === 'secondary') baseClasses = buttonVariants.secondary;
	else if (variant === 'ghost') baseClasses = buttonVariants.ghost;
	else if (variant === 'danger') baseClasses = buttonVariants.danger;
	else if (variant === 'success') baseClasses = buttonVariants.success;
	else if (variant === 'warning') baseClasses = buttonVariants.warning;
	else if (variant === 'accent') baseClasses = buttonVariants.accent;
	else if (variant === 'neutral') baseClasses = buttonVariants.neutral;
	else baseClasses = buttonVariants.primary;

	// Add size modifier for non-default sizes
	if (size === 'xs') return `${baseClasses} btn-xs`;
	if (size === 'sm') return `${baseClasses} btn-sm`;
	if (size === 'lg') return `${baseClasses} btn-lg`;

	return baseClasses;
}
