/**
 * Semantic design tokens for common patterns
 */

export const semantic = {
	// Page structure
	page: {
		container: 'mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8',
		header: 'flex items-center justify-between',
		content: 'space-y-4'
	},

	// Form structure
	form: {
		field: 'space-y-1',
		label: 'label',
		error: 'text-error text-sm',
		help: 'text-xs opacity-60'
	},

	// Status indicators
	status: {
		error: 'text-error',
		success: 'text-success',
		warning: 'text-warning',
		info: 'text-info'
	}
} as const;
