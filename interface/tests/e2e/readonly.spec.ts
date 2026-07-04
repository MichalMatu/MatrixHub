import { test, expect } from '@playwright/test';
import { authenticateByApi, waitForAuthenticatedShell } from './test-utils';

test.describe('Read-Only Scenarios', () => {
	test.beforeEach(async ({ page }) => {
		await authenticateByApi(page);
		await page.goto('/');
		await waitForAuthenticatedShell(page);
	});

	test('System Status renders status panels', async ({ page }) => {
		// Navigate to System Status
		// Note: System is a submenu, but we can browse directly to be fast,
		// or verify menu interaction. Direct navigation is more robust for "Status" testing.
		await page.goto('/system/status');
		await waitForAuthenticatedShell(page);

		await expect(page.getByText('System Status').first()).toBeVisible();
		await expect(page.getByRole('heading', { name: 'System Health' })).toBeVisible();
		await expect(page.getByRole('heading', { name: 'Hardware / Firmware' })).toBeVisible();
	});

	test('Language Switching (EN <-> PL)', async ({ page }) => {
		// 1. Ensure we are in English first (default)
		// Click EN button if it exists/is clickable
		// The buttons are in the sidebar: text="EN" / text="PL"

		const btnPL = page.getByRole('button', { name: 'PL' });
		const btnEN = page.getByRole('button', { name: 'EN' });
		await expect(btnPL).toBeVisible();
		await expect(btnEN).toBeVisible();

		// Switch to Polish
		await btnPL.click();
		await page.waitForTimeout(500); // Wait for language switch
		await expect(btnPL).toHaveClass(/active|selected|primary|btn-active/);

		// Menu text check
		await expect(page.locator('a', { hasText: 'Wykresy' }).first()).toBeVisible();

		// Switch back to English
		await btnEN.click();
		await page.waitForTimeout(500); // Wait for language switch
		await expect(btnEN).toHaveClass(/active|selected|primary|btn-active/);

		// Verify change back
		await expect(page.locator('a', { hasText: 'Charts' }).first()).toBeVisible();
	});
});
