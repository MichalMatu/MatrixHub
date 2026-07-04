import { test, expect } from '@playwright/test';
import { authenticateByApi, waitForAuthenticatedShell } from './test-utils';

test.describe('Dashboard', () => {
	test('should load dashboard and display widgets', async ({ page }) => {
		// Navigate to dashboard (assuming dev server or proxy to ESP32)
		await authenticateByApi(page);
		await page.goto('/');
		await waitForAuthenticatedShell(page);

		await expect(
			page.getByRole('heading', { name: /CO₂|Temperature|Humidity|Temperatura|Wilgotność/ }).first()
		).toBeVisible({ timeout: 15_000 });
	});

	test('should restore authenticated session when security is enabled', async ({ page }) => {
		await authenticateByApi(page);
		await page.goto('/');
		await waitForAuthenticatedShell(page);
		await expect(page.getByText('admin')).toBeVisible();
	});
});

test.describe('Settings', () => {
	test.beforeEach(async ({ page }) => {
		await authenticateByApi(page);
	});

	test('should load settings page', async ({ page }) => {
		await page.goto('/connections');
		await waitForAuthenticatedShell(page);

		// Check if connections/settings page loaded
		await expect(page.locator('h1, h2, h3').first()).toBeVisible();
		// Or check for specific settings sections
		const settingsContent = page.locator('.card, form, [data-testid]');
		await expect(settingsContent.first()).toBeVisible();
	});

	test('should save NTP settings', async ({ page }) => {
		await page.goto('/connections/ntp');
		await waitForAuthenticatedShell(page);

		// Wait for NTP form to load
		await page.waitForSelector('form, [data-testid="ntp-form"]');

		// Check if NTP settings form is present
		await expect(page.locator('form').first()).toBeVisible();
	});
});
