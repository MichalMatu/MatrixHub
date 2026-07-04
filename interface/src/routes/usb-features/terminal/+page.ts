import type { PageLoad } from './$types';

export const load = (async () => {
	return {
		title: 'USB Terminal'
	};
}) satisfies PageLoad;
