import type { PageLoad } from './$types';
import { redirect } from '@sveltejs/kit';

export const load = (async () => {
	redirect(307, '/');
}) satisfies PageLoad;
