import { test, expect, type Page, type Route } from '@playwright/test';

type MatrixSettings = {
	brightness: number;
	alarm_mode: number;
	rotation: number;
	auto_rotate: boolean;
	effect_enabled: boolean;
	effect_engine: number;
	effect_mode: number;
	effect_speed: number;
	effect_color: number;
	effect_color_2: number;
	effect_color_3: number;
	effect_reactivity_provider: number;
	effect_reactivity_gain: number;
	background_mode: number;
	data_visualization_enabled: boolean;
	data_visualization_source: number;
	data_visualization_metric: number;
	data_visualization_mode: number;
	data_visualization_min: number;
	data_visualization_max: number;
	data_visualization_color_min: number;
	data_visualization_color_mid: number;
	data_visualization_color_max: number;
	data_visualization_brightness_min: number;
	data_visualization_brightness_max: number;
	data_visualization_smoothing: number;
	data_visualization_stale_behavior: number;
	data_visualization_device_id: string;
	menu_enabled: boolean;
	menu_text_color: number;
	menu_scroll_speed: number;
	custom_icons?: number[][];
};

const initialMatrixSettings: MatrixSettings = {
	brightness: 20,
	alarm_mode: 1,
	rotation: 0,
	auto_rotate: false,
	effect_enabled: true,
	effect_engine: 0,
	effect_mode: 2,
	effect_speed: 1000,
	effect_color: 0x00ff00,
	effect_color_2: 0xff0000,
	effect_color_3: 0x0000ff,
	effect_reactivity_provider: 0,
	effect_reactivity_gain: 80,
	background_mode: 0,
	data_visualization_enabled: false,
	data_visualization_source: 0,
	data_visualization_metric: 0,
	data_visualization_mode: 0,
	data_visualization_min: 400,
	data_visualization_max: 2000,
	data_visualization_color_min: 0x00ff80,
	data_visualization_color_mid: 0xffd166,
	data_visualization_color_max: 0xff3000,
	data_visualization_brightness_min: 12,
	data_visualization_brightness_max: 180,
	data_visualization_smoothing: 50,
	data_visualization_stale_behavior: 0,
	data_visualization_device_id: '',
	menu_enabled: true,
	menu_text_color: 0xffffff,
	menu_scroll_speed: 20,
	custom_icons: [[], [], []]
};

function createTestToken() {
	const header = Buffer.from(JSON.stringify({ alg: 'none', typ: 'JWT' })).toString('base64url');
	const payload = Buffer.from(JSON.stringify({ username: 'admin', admin: true })).toString(
		'base64url'
	);
	return `${header}.${payload}.matrixhub-e2e`;
}

async function fulfillJson(route: Route, payload: unknown, status = 200) {
	await route.fulfill({
		status,
		contentType: 'application/json',
		body: JSON.stringify(payload)
	});
}

async function installAuthenticatedSession(page: Page) {
	const token = createTestToken();
	await page.addInitScript((accessToken) => {
		window.localStorage.setItem(
			'user',
			JSON.stringify({
				username: 'admin',
				admin: true,
				bearer_token: accessToken
			})
		);
	}, token);
}

test.describe('Matrix LED settings', () => {
	test('saves matrix display, effect, and alarm drafts through their own screens', async ({
		page
	}) => {
		let matrixSettings = structuredClone(initialMatrixSettings);
		const matrixPosts: Partial<MatrixSettings>[] = [];

		await installAuthenticatedSession(page);
		await page.route('**/rest/features', (route) =>
			fulfillJson(route, { ntp: true, sleep: true, file_manager: true })
		);
		await page.route('**/api/matrix/settings', async (route) => {
			if (route.request().method() === 'GET') {
				await fulfillJson(route, matrixSettings);
				return;
			}

			if (route.request().method() === 'POST') {
				const payload = (route.request().postDataJSON() ?? {}) as Partial<MatrixSettings>;
				matrixSettings = { ...matrixSettings, ...payload };
				matrixPosts.push(structuredClone(payload));
				await fulfillJson(route, matrixSettings);
				return;
			}

			await route.continue();
		});

		await page.goto('/system/matrix');

		await expect(page.getByRole('heading', { name: 'Matrix Display' })).toBeVisible({
			timeout: 30_000
		});
		const matrixTabs = page.getByLabel('Matrix LED sections');
		await expect(matrixTabs.getByRole('link', { name: 'Alarms' })).toBeVisible();
		await expect(matrixTabs.getByRole('link', { name: 'Effects' })).toBeVisible();

		const brightnessSlider = page.getByLabel('Brightness');
		await brightnessSlider.evaluate((input) => {
			const range = input as HTMLInputElement;
			range.value = '88';
			range.dispatchEvent(new Event('input', { bubbles: true }));
		});
		await page.getByRole('button', { name: '90°' }).click();
		await page.getByLabel('Menu Text Color').fill('#112233');

		expect(matrixPosts).toHaveLength(0);

		await page.getByRole('button', { name: 'Save' }).click();
		await expect.poll(() => matrixPosts.length).toBe(1);

		expect(matrixPosts[0]).toMatchObject({
			brightness: 88,
			rotation: 1,
			menu_text_color: 0x112233,
			menu_enabled: true
		});
		expect(matrixPosts[0]).not.toHaveProperty('effect_mode');
		expect(matrixPosts[0]).not.toHaveProperty('alarm_mode');

		await page.goto('/system/matrix/effects');
		await expect(page.getByRole('heading', { name: 'Visual Effects' })).toBeVisible({
			timeout: 30_000
		});
		await page.getByRole('combobox', { name: 'Effect Mode' }).selectOption('11');
		await page.getByRole('button', { name: 'Save' }).click();
		await expect.poll(() => matrixPosts.length).toBe(2);
		expect(matrixPosts[1]).toMatchObject({
			effect_enabled: true,
			effect_engine: 0,
			effect_mode: 11,
			effect_speed: 1000,
			effect_reactivity_provider: 0,
			effect_reactivity_gain: 80
		});
		expect(matrixPosts[1]).not.toHaveProperty('brightness');
		expect(matrixPosts[1]).not.toHaveProperty('alarm_mode');

		await page.goto('/system/matrix/alarms');
		await expect(page.getByRole('heading', { name: 'Matrix Alarms' })).toBeVisible({
			timeout: 30_000
		});
		await page.getByRole('button', { name: 'Edit Icons' }).click();
		const iconDialog = page.getByRole('dialog');
		await iconDialog.getByRole('button', { name: 'Pixel 1', exact: true }).click();
		await iconDialog.getByRole('button', { name: 'Save' }).click();

		expect(matrixPosts).toHaveLength(2);
		await page.getByRole('button', { name: 'Save' }).first().click();
		await expect.poll(() => matrixPosts.length).toBe(3);
		expect(matrixPosts[2]).toMatchObject({ alarm_mode: 1 });
		expect(matrixPosts[2].custom_icons?.[0]).toHaveLength(64);
		expect(matrixPosts[2]).not.toHaveProperty('effect_mode');
		expect(matrixPosts[2]).not.toHaveProperty('brightness');
	});

	test('guards unsaved matrix drafts during internal navigation', async ({ page }) => {
		let matrixSettings = structuredClone(initialMatrixSettings);

		await installAuthenticatedSession(page);
		await page.route('**/rest/features', (route) =>
			fulfillJson(route, { ntp: true, sleep: true, file_manager: true })
		);
		await page.route('**/api/matrix/settings', async (route) => {
			if (route.request().method() === 'GET') {
				await fulfillJson(route, matrixSettings);
				return;
			}

			if (route.request().method() === 'POST') {
				const payload = (route.request().postDataJSON() ?? {}) as Partial<MatrixSettings>;
				matrixSettings = { ...matrixSettings, ...payload };
				await fulfillJson(route, matrixSettings);
				return;
			}

			await route.continue();
		});

		await page.goto('/system/matrix');
		await expect(page.getByRole('heading', { name: 'Matrix Display' })).toBeVisible({
			timeout: 30_000
		});

		const brightnessSlider = page.getByLabel('Brightness');
		await brightnessSlider.evaluate((input) => {
			const range = input as HTMLInputElement;
			range.value = '77';
			range.dispatchEvent(new Event('input', { bubbles: true }));
		});

		await page.getByRole('link', { name: 'Charts' }).click();
		const unsavedDialog = page.getByRole('dialog', { name: 'Unsaved changes' });
		await expect(unsavedDialog).toBeVisible();
		await expect(unsavedDialog.getByText('You have unsaved changes')).toBeVisible();
		await unsavedDialog.getByRole('button', { name: 'Stay' }).click();
		await expect(page).toHaveURL(/\/system\/matrix$/);

		await page.getByRole('link', { name: 'Charts' }).click();
		await page
			.getByRole('dialog', { name: 'Unsaved changes' })
			.getByRole('button', { name: 'Leave page' })
			.click();
		await expect(page).toHaveURL(/\/charts$/);
	});
});
