import type { LayoutLoad } from './$types';

// This can be false if you're using a fallback (i.e. SPA mode)
export const prerender = false;
export const ssr = false;

export const load = (async () => {
	return {
		title: 'MatrixHub',
		github: 'MichalMatu/MatrixHub',
		copyright: '2025 MatrixHub',
		appName: 'MatrixHub'
	};
}) satisfies LayoutLoad;
