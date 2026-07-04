import type { PageLoad } from './$types';

export const load = (async () => {
	return {
		title: 'Time'
	};
}) satisfies PageLoad;
