/**
 * Icon design tokens
 */

export const icon = {
	wrapper: {
		stat: 'mask mask-hexagon flex h-10 w-10 flex-none items-center justify-center',
		list: 'flex-none rounded-xl bg-base-200/80 p-2 text-base-content',
		panel:
			'flex h-[72px] w-[72px] min-h-[72px] min-w-[72px] items-center justify-center rounded-2xl border-2 text-3xl'
	},
	size: {
		sm: 'h-4 w-4',
		md: 'h-5 w-5',
		lg: 'h-6 w-6',
		xl: 'h-7 w-7',
		'2xl': 'h-10 w-10'
	}
} as const;
