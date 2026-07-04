import type { PageLoad } from './$types';

export const load = (async () => {
	return { title: 'Power Settings' };
}) satisfies PageLoad;
