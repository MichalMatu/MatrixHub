import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useSessionAccess } from './useSessionAccess.svelte';

const { mockUser } = vi.hoisted(() => ({
	mockUser: {
		bearer_token: '',
		admin: false,
		isValid: false,
		username: '',
		authNotice: null,
		invalidate: vi.fn()
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

describe('useSessionAccess', () => {
	beforeEach(() => {
		mockUser.bearer_token = '';
		mockUser.admin = false;
		mockUser.isValid = false;
		mockUser.username = '';
		mockUser.authNotice = null;
		mockUser.invalidate.mockReset();
	});

	it('blocks read and manage access when user is logged out', () => {
		const cleanup = $effect.root(() => {
			const session = useSessionAccess();

			expect(session.isAuthenticated).toBe(false);
			expect(session.isAdmin).toBe(false);
			expect(session.canRead).toBe(false);
			expect(session.canManage).toBe(false);
			expect(session.apiOptions).toMatchObject({
				bearerToken: ''
			});
		});

		cleanup();
	});

	it('allows read access but not manage access for authenticated non-admin users', () => {
		mockUser.bearer_token = 'token';
		mockUser.isValid = true;

		const cleanup = $effect.root(() => {
			const session = useSessionAccess();

			expect(session.isAuthenticated).toBe(true);
			expect(session.isAdmin).toBe(false);
			expect(session.canRead).toBe(true);
			expect(session.canManage).toBe(false);
			expect(session.apiOptions).toMatchObject({
				bearerToken: 'token'
			});
			expect(typeof session.apiOptions.onUnauthorized).toBe('function');
		});

		cleanup();
	});

	it('allows manage access for authenticated admins', () => {
		mockUser.bearer_token = 'token';
		mockUser.isValid = true;
		mockUser.admin = true;

		const cleanup = $effect.root(() => {
			const session = useSessionAccess();

			expect(session.isAuthenticated).toBe(true);
			expect(session.isAdmin).toBe(true);
			expect(session.canRead).toBe(true);
			expect(session.canManage).toBe(true);
			expect(session.username).toBe('');
		});

		cleanup();
	});

	it('delegates invalidate through the session facade and apiOptions handler', () => {
		const cleanup = $effect.root(() => {
			const session = useSessionAccess();

			session.invalidate('unauthorized');
			session.apiOptions.onUnauthorized?.();

			expect(mockUser.invalidate).toHaveBeenCalledTimes(2);
			expect(mockUser.invalidate).toHaveBeenNthCalledWith(1, 'unauthorized');
			expect(mockUser.invalidate).toHaveBeenNthCalledWith(2, 'unauthorized');
		});

		cleanup();
	});
});
