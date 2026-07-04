import { test, expect } from '@playwright/test';
import { authenticateByApi, waitForAuthenticatedShell } from './test-utils';

test('smoke: app renders (and can login when security enabled)', async ({ page }) => {
	await authenticateByApi(page);
	await page.goto('/');
	await waitForAuthenticatedShell(page);

	// Verify navigation to Charts
	const chartsLink = page.locator('a[href="/charts"]').first();

	// Ensure header/menu is visible; on smaller screens checking visibility might need menu toggle
	if (await chartsLink.isVisible()) {
		await chartsLink.click();
	} else {
		// Possibly inside mobile menu
		await page.goto('/charts');
	}

	await expect(page).toHaveURL(/.*\/charts/);

	await expect(page.locator('.drawer-content')).toBeVisible();
});
