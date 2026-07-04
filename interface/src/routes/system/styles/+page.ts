import type { PageLoad } from './$types';

export const load = (async () => {
	return { title: 'Theme & Styles' };
}) satisfies PageLoad;
