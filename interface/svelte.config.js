let vitePreprocess;
try {
	const { vitePreprocess: vp } = await import('@sveltejs/vite-plugin-svelte');
	vitePreprocess = vp;
} catch {
	console.warn('Warning: Could not load @sveltejs/vite-plugin-svelte. Linting may be limited.');
	vitePreprocess = () => {};
}

let adapter;
try {
	const { default: staticAdapter } = await import('@sveltejs/adapter-static');
	adapter = staticAdapter;
} catch {
	// Squelch error for language server if optional dependencies fail to resolve per
	// https://github.com/npm/cli/issues/4828
	console.warn('Warning: Could not load @sveltejs/adapter-static. Using fallback.');
	adapter = () => ({
		name: '@sveltejs/adapter-static',
		adapt: () => {}
	});
}

/** @type {import('@sveltejs/kit').Config} */
const config = {
	// Consult https://kit.svelte.dev/docs/integrations#preprocessors
	// for more information about preprocessors
	preprocess: vitePreprocess(),

	kit: {
		adapter: adapter({
			pages: 'build',
			assets: 'build',
			fallback: 'index.html',
			precompress: false,
			strict: true
		}),
		alias: {
			$src: './src',
			'@matrixhub/device-sdk': '../packages/device-sdk/src/index.ts'
		},
		output: {
			bundleStrategy: 'split'
		}
	}
};

export default config;
