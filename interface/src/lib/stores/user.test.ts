import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

function createJwt(extraPayload: Record<string, unknown> = {}) {
	const header = btoa(JSON.stringify({ alg: 'HS256', typ: 'JWT' }));
	const payload = btoa(JSON.stringify({ username: 'admin', admin: true, ...extraPayload }));
	return `${header}.${payload}.signature`;
}

describe('UserStore', () => {
	let cookieAssignments: string[] = [];
	let currentCookie = '';
	let originalCookieDescriptor: PropertyDescriptor | undefined;

	beforeEach(() => {
		vi.resetModules();
		vi.clearAllMocks();
		localStorage.clear();
		cookieAssignments = [];
		currentCookie = '';
		originalCookieDescriptor =
			Object.getOwnPropertyDescriptor(Document.prototype, 'cookie') ??
			Object.getOwnPropertyDescriptor(HTMLDocument.prototype, 'cookie');

		Object.defineProperty(document, 'cookie', {
			configurable: true,
			get() {
				return currentCookie;
			},
			set(value: string) {
				cookieAssignments.push(value);

				const segments = value.split(';').map((segment) => segment.trim());
				const pair = segments[0] ?? '';
				const pathAttr = segments.find((segment) => segment.toLowerCase().startsWith('path='));

				if (value.includes('Max-Age=0')) {
					currentCookie = '';
					return;
				}

				if (pathAttr === 'path=/') {
					currentCookie = pair;
				}
			}
		});
	});

	afterEach(() => {
		if (originalCookieDescriptor) {
			Object.defineProperty(document, 'cookie', originalCookieDescriptor);
		}
		vi.unstubAllGlobals();
	});

	it('removes malformed stored tokens during initialization', async () => {
		localStorage.setItem(
			'user',
			JSON.stringify({
				username: 'admin',
				admin: true,
				bearer_token: 'not-a-jwt'
			})
		);

		const { user } = await import('./user.svelte');

		expect(user.bearer_token).toBe('');
		expect(localStorage.getItem('user')).toBeNull();
		expect(document.cookie).not.toContain('access_token=');
		expect(cookieAssignments).toContain('access_token=; Max-Age=0; path=/ws; SameSite=Strict');
		expect(cookieAssignments).toContain('access_token=; Max-Age=0; path=/; SameSite=Strict');
	});

	it('restores a valid stored token without local expiry checks', async () => {
		const token = createJwt();

		localStorage.setItem(
			'user',
			JSON.stringify({
				username: 'admin',
				admin: true,
				bearer_token: token
			})
		);

		const { user } = await import('./user.svelte');

		expect(user.bearer_token).toBe(token);
		expect(user.username).toBe('admin');
		expect(user.admin).toBe(true);
		expect(user.isValid).toBe(true);
		expect(cookieAssignments).toContain('access_token=; Max-Age=0; path=/; SameSite=Strict');
		expect(cookieAssignments).toContain(
			`access_token=${encodeURIComponent(token)}; path=/ws; SameSite=Strict`
		);
	});

	it('keeps the session until it is explicitly invalidated', async () => {
		const token = createJwt();

		const { user } = await import('./user.svelte');
		const { goto } = await import('$app/navigation');

		user.init(token);

		expect(user.bearer_token).toBe(token);
		expect(user.isValid).toBe(true);

		user.invalidate('unauthorized');

		expect(user.bearer_token).toBe('');
		expect(user.authNotice).toBe('unauthorized');
		expect(localStorage.getItem('user')).toBeNull();
		expect(document.cookie).not.toContain('access_token=');
		expect(goto).toHaveBeenCalledWith('/');
	});

	it('stores an auth notice when invalidated manually', async () => {
		const token = createJwt();

		const { user } = await import('./user.svelte');
		const { goto } = await import('$app/navigation');

		user.init(token);
		user.invalidate('unauthorized');

		expect(user.bearer_token).toBe('');
		expect(user.authNotice).toBe('unauthorized');
		expect(goto).toHaveBeenCalledWith('/');

		user.init(token);
		expect(user.authNotice).toBeNull();
	});

	it('adds Secure to WS auth cookies on HTTPS pages', async () => {
		vi.stubGlobal('location', { protocol: 'https:' });
		const token = createJwt();

		const { user } = await import('./user.svelte');

		user.init(token);

		expect(cookieAssignments).toContain(
			'access_token=; Max-Age=0; path=/; SameSite=Strict; Secure'
		);
		expect(cookieAssignments).toContain(
			`access_token=${encodeURIComponent(token)}; path=/ws; SameSite=Strict; Secure`
		);
	});
});
