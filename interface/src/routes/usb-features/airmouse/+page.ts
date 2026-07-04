import type { PageLoad } from './$types';

export const load = (() => {
	return {
		title: 'Air Mouse'
	};
}) satisfies PageLoad;
