import type { Plugin } from 'vite';
import type { OutputOptions } from 'rollup';

function shortenHashPlaceholders(pattern: string): string {
	return pattern.replace(/\[hash(?::\d+)?\]/g, '[hash:8]');
}

function normalizeOutputOptions(output: OutputOptions): OutputOptions {
	const { assetFileNames, chunkFileNames, entryFileNames } = output;
	if (typeof assetFileNames !== 'string') return output;

	const normalized: OutputOptions = {
		...output,
		assetFileNames: shortenHashPlaceholders(assetFileNames)
	};

	if (typeof chunkFileNames === 'string' && chunkFileNames.includes('hash')) {
		normalized.chunkFileNames = shortenHashPlaceholders(chunkFileNames);
		normalized.entryFileNames =
			typeof entryFileNames === 'string' ? shortenHashPlaceholders(entryFileNames) : entryFileNames;
	}

	return normalized;
}

export default function viteLittleFS(): Plugin[] {
	return [
		{
			name: 'vite-plugin-littlefs',
			enforce: 'post',
			apply: 'build',

			async config(config) {
				const buildConfig = config.build?.rollupOptions;
				const output = buildConfig?.output;
				if (!buildConfig || !output) return;

				if (Array.isArray(output)) {
					buildConfig.output = output.map(normalizeOutputOptions);
				} else {
					buildConfig.output = normalizeOutputOptions(output);
				}
			}
		}
	];
}
