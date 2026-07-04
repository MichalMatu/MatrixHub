/**
 * Layout component variants (cards, dialogs, forms)
 */

// Card variants
export const cardVariants = {
	base: 'card bg-base-100 shadow-sm border border-base-300/60',
	interactive:
		'card bg-base-100 shadow-sm border border-base-300/60 hover:shadow-md transition-shadow',
	outline: 'card bg-base-100 border border-base-300'
} as const;

// Dialog variants
export const dialogVariants = {
	base: 'rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit flex-col justify-between p-4 shadow-lg',
	sm: 'rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-sm flex-col justify-between p-4 shadow-lg',
	md: 'rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg',
	lg: 'rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-lg flex-col justify-between p-4 shadow-lg'
} as const;

// Form layout variants
export const formVariants = {
	grid: 'grid w-full grid-cols-1 content-center gap-x-4 px-4 gap-y-2 sm:grid-cols-2',
	fieldset: 'fieldset w-full',
	actions: 'flex flex-wrap justify-end gap-2 mt-4'
} as const;
