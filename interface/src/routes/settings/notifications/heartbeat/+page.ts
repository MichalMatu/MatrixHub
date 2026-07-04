import type { PageLoad } from './$types';

export const load = (() => {
	return {
		title: 'Heartbeat Monitor'
	};
}) satisfies PageLoad;
