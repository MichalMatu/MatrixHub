import { sveltekit } from '@sveltejs/kit/vite';
import { resolve } from 'node:path';
import { createLogger, defineConfig, type Logger, type UserConfig } from 'vite';
import Icons from 'unplugin-icons/vite';
import viteLittleFS from './vite-plugin-littlefs';
import tailwindcss from '@tailwindcss/vite';
import { visualizer } from 'rollup-plugin-visualizer';
import viteCompression from 'vite-plugin-compression';

const defaultProxyTarget =
	process.env.VITE_PROXY_TARGET || process.env.DEVICE_URL || 'https://192.168.0.18';
const devHost = process.env.VITE_DEV_HOST || '0.0.0.0';
const hmrHost = process.env.VITE_HMR_HOST;
const chromeDevtoolsConfigPath = '/.well-known/appspecific/com.chrome.devtools.json';
const expectedProxyDisconnectCodes = new Set(['ECONNRESET', 'EPIPE']);
// Generating stats.html is useful for one-off bundle analysis, but it adds work
// to every build, so keep it behind an explicit analyze mode.

function toWebSocketTarget(target: string) {
	return target.replace(/^http:/, 'ws:').replace(/^https:/, 'wss:');
}

function createQuietProxyLogger(): Logger {
	const logger = createLogger();
	const originalError = logger.error.bind(logger);

	logger.error = (msg, options) => {
		const errorCode = options?.error && 'code' in options.error ? options.error.code : undefined;
		const isExpectedProxyDisconnect =
			typeof msg === 'string' &&
			msg.startsWith('ws proxy error:') &&
			typeof errorCode === 'string' &&
			expectedProxyDisconnectCodes.has(errorCode);

		if (isExpectedProxyDisconnect) {
			logger.warn(
				`ws proxy disconnected (${errorCode}) - check device restart, Wi-Fi or VITE_PROXY_TARGET`,
				{ timestamp: true }
			);
			return;
		}

		originalError(msg, options);
	};

	return logger;
}

const config = defineConfig(({ mode }) => {
	const isAnalyzeBuild = mode === 'analyze';
	const plugins: NonNullable<UserConfig['plugins']> = [
		{
			name: 'chrome-devtools-json-stub',
			configureServer(server) {
				server.middlewares.use((req, res, next) => {
					if (req.url !== chromeDevtoolsConfigPath) {
						next();
						return;
					}

					res.statusCode = 204;
					res.end();
				});
			}
		},
		// Paraglide files are generated once by the explicit `npm run i18n:build`
		// step before Vite starts, so we avoid compiling them again for SSR and client.
		sveltekit(),
		Icons({
			compiler: 'svelte'
		}),
		// Tailwind's optimize step uses LightningCSS and currently emits a warning for DaisyUI's
		// `@property --radialprogress` at-rule. Disable optimize to keep builds clean.
		tailwindcss({ optimize: false }),
		// Keep .gz companions because the firmware embed step prefers already
		// compressed assets when converting the frontend into WWWData.h.
		viteCompression({
			algorithm: 'gzip',
			ext: '.gz',
			deleteOriginFile: false,
			threshold: 1024
		}),
		// Shorten file names for LittleFS 32 char limit
		viteLittleFS()
	];

	if (isAnalyzeBuild) {
		// Only emit the heavy treemap report when we explicitly ask for it via
		// `npm run build:analyze`.
		plugins.push(
			visualizer({ filename: 'stats.html', template: 'treemap', gzipSize: true, brotliSize: true })
		);
	}

	return {
		customLogger: createQuietProxyLogger(),
		plugins,
		resolve: {
			alias: {
				'svelte-focus-trap': 'svelte-focus-trap/src/index.js',
				'@matrixhub/device-sdk': resolve(__dirname, '../packages/device-sdk/src/index.ts')
			}
		},
		server: {
			host: devHost,
			port: 5173,
			...(hmrHost
				? {
						// Override only when explicitly provided via env var.
						hmr: {
							host: hmrHost,
							port: 5173,
							protocol: 'ws'
						}
					}
				: {}),
			proxy: {
				'/api': {
					target: defaultProxyTarget,
					changeOrigin: true,
					secure: false, // Allow self-signed certs for local mDNS
					// CRITICAL: For binary endpoints, we must NOT let the proxy
					// transform the response body in any way.
					configure: (proxy, _options) => {
						proxy.on('error', (err, _req, _res) => {
							console.error('Proxy error:', err);
						});
						proxy.on('proxyRes', (proxyRes, req, res) => {
							// Binary endpoints that return raw bytes
							const binaryPaths = ['/api/logs/download', '/api/ble/thermometers/binary'];
							const isBinary = binaryPaths.some((p) => req.url?.includes(p));
							if (isBinary) {
								// Ensure proper headers for binary data
								res.setHeader(
									'Content-Type',
									proxyRes.headers['content-type'] || 'application/octet-stream'
								);
								console.log(`[Proxy Binary] ${req.url} - ${proxyRes.statusCode}`);
							}
						});
					}
				},
				'/rest': {
					target: defaultProxyTarget,
					changeOrigin: true,
					secure: false
				},
				'/ws': {
					target: toWebSocketTarget(defaultProxyTarget),
					changeOrigin: true,
					ws: true,
					secure: false
				}
			}
		},
		build: {
			target: 'esnext',
			cssTarget: 'chrome120',
			cssMinify: 'esbuild',
			minify: 'terser',
			sourcemap: false,
			rollupOptions: {
				output: {
					experimentalMinChunkSize: 12_000
				}
			}
		}
	};
});

export default config;
