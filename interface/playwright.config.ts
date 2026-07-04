import { defineConfig, devices } from '@playwright/test';

const deviceUrl = process.env.DEVICE_URL ?? 'https://192.168.0.18';
const port = Number(process.env.E2E_PORT ?? 5174);
const host = process.env.E2E_HOST ?? '127.0.0.1';

export default defineConfig({
	testDir: './tests/e2e',
	timeout: 60_000,
	expect: { timeout: 10_000 },
	fullyParallel: true,
	workers: 1,
	retries: process.env.CI ? 1 : 0,
	use: {
		baseURL: `http://${host}:${port}`,
		trace: 'retain-on-failure',
		screenshot: 'only-on-failure',
		video: 'retain-on-failure'
	},
	webServer: {
		command: `npm run dev -- --host ${host} --port ${port}`,
		url: `http://${host}:${port}`,
		reuseExistingServer: !process.env.CI,
		timeout: 60_000,
		env: {
			...process.env,
			VITE_PROXY_TARGET: deviceUrl
		}
	},
	projects: [
		{
			name: 'chromium',
			use: { ...devices['Desktop Chrome'] }
		}
	]
});
