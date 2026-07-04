/// <reference types="vitest" />
import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import Icons from 'unplugin-icons/vite';
import { resolve } from 'path';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export default defineConfig(<any>{
	plugins: [
		// Tests consume the generated $lib/paraglide files after the explicit
		// `npm run i18n:build` step wired in package.json.
		svelte({
			hot: !process.env.VITEST,
			compilerOptions: {
				runes: true
			}
		}),
		Icons({
			compiler: 'svelte'
		})
	],
	test: {
		environment: 'jsdom',
		globals: true,
		setupFiles: ['./vitest.setup.ts'],
		include: ['src/**/*.{test,spec}.{js,ts}', 'src/**/*.{test,spec}.svelte.{js,ts}'],
		alias: {
			$app: {
				environment: resolve('./src/mocks/appEnvironment.ts'),
				navigation: resolve('./src/mocks/appNavigation.ts'),
				paths: resolve('./.svelte-kit/runtime/app/paths'),
				stores: resolve('./.svelte-kit/runtime/app/stores'),
				state: resolve('./src/mocks/appState.ts')
			}
		},
		coverage: {
			provider: 'v8',
			reporter: ['text', 'json', 'html'],
			exclude: [
				'node_modules/',
				'build/',
				'.svelte-kit/',
				'src/lib/paraglide/**',
				'**/*.config.*',
				'**/*.d.ts',
				'**/types/**'
			]
		}
	},
	resolve: {
		conditions: ['browser'],
		alias: {
			$lib: resolve('./src/lib'),
			'@matrixhub/device-sdk': resolve('./../packages/device-sdk/src/index.ts'),
			'$app/environment': resolve('./src/mocks/appEnvironment.ts'),
			'$app/state': resolve('./src/mocks/appState.ts'),
			'$app/navigation': resolve('./src/mocks/appNavigation.ts'),
			$app: resolve('./.svelte-kit/runtime/app')
		}
	}
});
