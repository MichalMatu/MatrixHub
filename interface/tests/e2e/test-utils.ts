import { type Page, expect } from '@playwright/test';

const defaultCredentialsDialogTitle =
	/Default Credentials Detected|Wykryto domyślne dane logowania/;
const defaultCredentialsDismissLabel = /Skip for now|Pomiń na teraz|Close|Zamknij/;
const usernameLabel = /Username|Nazwa użytkownika/;
const passwordLabel = /Password|Hasło/;
const signInLabel = /Sign In|Login|Zaloguj/;

let cachedAccessToken: string | null = null;

function loginUsernameInput(page: Page) {
	return page
		.locator('[data-testid="login-username"], #user')
		.or(page.getByRole('textbox', { name: usernameLabel }))
		.first();
}

function loginPasswordInput(page: Page) {
	return page
		.locator('[data-testid="login-password"], #pwd')
		.or(page.getByRole('textbox', { name: passwordLabel }))
		.first();
}

function loginSubmitButton(page: Page) {
	return page
		.locator('[data-testid="login-submit"], button[type="submit"]')
		.or(page.getByRole('button', { name: signInLabel }))
		.first();
}

export async function dismissDefaultCredentialsWarning(page: Page) {
	const dialog = page.getByRole('dialog').filter({ hasText: defaultCredentialsDialogTitle });

	try {
		await expect(dialog).toBeVisible({ timeout: 1500 });
	} catch {
		return;
	}

	await dialog.getByRole('button', { name: defaultCredentialsDismissLabel }).first().click();
	await expect(dialog).toBeHidden({ timeout: 5000 });
}

async function getAccessToken(page: Page) {
	if (cachedAccessToken) {
		return cachedAccessToken;
	}

	const username = process.env.TEST_USERNAME ?? 'admin';
	const password = process.env.TEST_PASSWORD ?? 'admin';
	const response = await page.request.post('/rest/signIn', {
		data: { username, password }
	});

	if (!response.ok()) {
		throw new Error(`E2E signIn failed: HTTP ${response.status()} ${await response.text()}`);
	}

	const body = (await response.json()) as { access_token?: unknown };
	if (typeof body.access_token !== 'string' || !body.access_token.trim()) {
		throw new Error('E2E signIn returned no access_token');
	}

	cachedAccessToken = body.access_token.trim();
	return cachedAccessToken;
}

export async function authenticateByApi(page: Page) {
	const accessToken = await getAccessToken(page);
	await page.addInitScript((token) => {
		const payloadPart = token.split('.')[1] ?? '';
		const normalizedPayload = payloadPart.replace(/-/g, '+').replace(/_/g, '/');
		const paddedPayload = normalizedPayload.padEnd(
			Math.ceil(normalizedPayload.length / 4) * 4,
			'='
		);
		const payload = JSON.parse(atob(paddedPayload));

		window.localStorage.setItem(
			'user',
			JSON.stringify({
				username: payload.username ?? 'admin',
				admin: !!payload.admin,
				bearer_token: token
			})
		);
		document.cookie = `access_token=${encodeURIComponent(token)}; path=/ws; SameSite=Strict`;
	}, accessToken);
}

export async function waitForAuthenticatedShell(page: Page) {
	const shellEntry = page
		.locator('.drawer, .drawer-content, a[href="/charts"], a[href="/alarms"]')
		.first();

	try {
		await expect(shellEntry).toBeVisible({ timeout: 20000 });
	} catch (error) {
		const bodyText = (
			await page
				.locator('body')
				.innerText()
				.catch(() => '')
		).trim();
		if (bodyText) {
			throw error;
		}

		await page.reload({ waitUntil: 'domcontentloaded' });
		await expect(shellEntry).toBeVisible({ timeout: 20000 });
	}
}

export async function loginAsAdmin(page: Page) {
	const username = process.env.TEST_USERNAME ?? 'admin';
	const password = process.env.TEST_PASSWORD ?? 'admin';

	// Wait for either the main layout OR the login form
	const drawer = page.locator('.drawer');
	const loginForm = loginUsernameInput(page);
	const appEntry = drawer.or(loginForm);

	try {
		await expect(appEntry).toBeVisible({ timeout: 15000 });
	} catch (e) {
		const bodyText = (
			await page
				.locator('body')
				.innerText()
				.catch(() => '')
		).trim();
		if (!bodyText) {
			await page.reload({ waitUntil: 'domcontentloaded' });
			await expect(appEntry).toBeVisible({ timeout: 15000 });
		} else {
			console.log(
				`Timeout waiting for drawer or login form. Body starts with: ${bodyText.slice(0, 160)}`
			);
			throw e;
		}
	}

	if (await loginForm.isVisible()) {
		await loginForm.fill(username);
		await loginPasswordInput(page).fill(password);
		await loginSubmitButton(page).click();
		await expect(drawer).toBeVisible({ timeout: 15000 });
	} else {
		await expect(drawer).toBeVisible({ timeout: 15000 });
	}

	await dismissDefaultCredentialsWarning(page);
}
