import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useLogin } from './useLogin.svelte';

function createDeferred<T>() {
	let resolve!: (value: T) => void;
	let reject!: (reason?: unknown) => void;
	const promise = new Promise<T>((res, rej) => {
		resolve = res;
		reject = rej;
	});

	return { promise, resolve, reject };
}

const { mockNotifications, mockSignIn, mockUser, mockErrorHandle } = vi.hoisted(() => ({
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn()
	},
	mockSignIn: vi.fn(),
	mockUser: {
		username: '',
		bearer_token: '',
		authNotice: null,
		init: vi.fn((token: string) => {
			mockUser.username = 'admin';
			mockUser.bearer_token = token;
			mockUser.authNotice = null;
		})
	},
	mockErrorHandle: vi.fn()
}));
const mockOpenDefaultCredentialsWarning = vi.hoisted(() => vi.fn());

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/services/api/core/SecurityApiService', () => ({
	signIn: mockSignIn
}));

vi.mock('$lib/services/ui/ErrorService', () => ({
	ErrorService: {
		handle: mockErrorHandle
	}
}));

vi.mock('./defaultCredentialsWarning', () => ({
	openDefaultCredentialsWarning: mockOpenDefaultCredentialsWarning
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	toast_login_success: ({ username }: { username: string }) => `User ${username} signed in`,
	toast_login_failed: () => 'Wrong Username or Password!',
	toast_login_error: () => 'Login failed.'
}));

describe('useLogin', () => {
	beforeEach(() => {
		vi.useRealTimers();
		mockNotifications.success.mockReset();
		mockNotifications.error.mockReset();
		mockSignIn.mockReset();
		mockUser.username = '';
		mockUser.bearer_token = '';
		mockUser.authNotice = null;
		mockUser.init.mockClear();
		mockErrorHandle.mockReset();
		mockOpenDefaultCredentialsWarning.mockReset();
	});

	it('ignores duplicate submits while a login request is already pending', async () => {
		const deferred = createDeferred<{ access_token: string } | null>();
		mockSignIn.mockReturnValue(deferred.promise);

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		const firstAttempt = loginState.signInUser({ username: 'admin', password: 'secret' });
		const secondAttempt = loginState.signInUser({ username: 'admin', password: 'secret' });

		expect(loginState.isSubmitting).toBe(true);
		expect(mockSignIn).toHaveBeenCalledTimes(1);

		deferred.resolve({ access_token: 'jwt-token' });

		await firstAttempt;
		await secondAttempt;

		expect(loginState.isSubmitting).toBe(false);
		cleanup();
	});

	it('keeps the pending state visible long enough to show feedback on fast success', async () => {
		vi.useFakeTimers();
		mockSignIn.mockResolvedValue({ access_token: 'jwt-token' });

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		const signInPromise = loginState.signInUser({ username: 'admin', password: 'secret' });
		await Promise.resolve();

		expect(loginState.isSubmitting).toBe(true);

		await vi.advanceTimersByTimeAsync(200);
		expect(loginState.isSubmitting).toBe(true);

		await vi.advanceTimersByTimeAsync(300);
		await signInPromise;

		expect(loginState.isSubmitting).toBe(false);
		expect(mockNotifications.success).toHaveBeenCalledOnce();
		expect(mockOpenDefaultCredentialsWarning).not.toHaveBeenCalled();

		cleanup();
	});

	it('opens the default credentials warning after a successful admin/admin login', async () => {
		vi.useFakeTimers();
		mockSignIn.mockResolvedValue({ access_token: 'jwt-token' });

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		const signInPromise = loginState.signInUser({ username: 'admin', password: 'admin' });
		await Promise.resolve();
		await vi.advanceTimersByTimeAsync(500);
		await signInPromise;

		expect(mockOpenDefaultCredentialsWarning).toHaveBeenCalledOnce();

		cleanup();
	});

	it('does not open the default credentials warning for other successful credentials', async () => {
		vi.useFakeTimers();
		mockSignIn.mockResolvedValue({ access_token: 'jwt-token' });

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		const signInPromise = loginState.signInUser({ username: 'admin', password: 'secret' });
		await Promise.resolve();
		await vi.advanceTimersByTimeAsync(500);
		await signInPromise;

		expect(mockOpenDefaultCredentialsWarning).not.toHaveBeenCalled();

		cleanup();
	});

	it('does not open the default credentials warning when login fails', async () => {
		mockSignIn.mockResolvedValue(null);

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		loginState.username = 'admin';
		loginState.password = 'admin';

		await loginState.signInUser({ username: 'admin', password: 'admin' });

		expect(mockOpenDefaultCredentialsWarning).not.toHaveBeenCalled();
		expect(mockNotifications.error).toHaveBeenCalledOnce();
		expect(loginState.username).toBe('');
		expect(loginState.password).toBe('');

		cleanup();
	});

	it('routes unexpected sign-in errors to the error handler without clearing credentials', async () => {
		const loginError = new Error('Network down');
		mockSignIn.mockRejectedValue(loginError);

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		loginState.username = 'admin';
		loginState.password = 'secret';

		await loginState.signInUser({ username: 'admin', password: 'secret' });

		expect(mockErrorHandle).toHaveBeenCalledWith(loginError, 'Login failed.');
		expect(mockNotifications.error).not.toHaveBeenCalled();
		expect(loginState.username).toBe('admin');
		expect(loginState.password).toBe('secret');
		expect(loginState.isSubmitting).toBe(false);
		expect(mockOpenDefaultCredentialsWarning).not.toHaveBeenCalled();

		cleanup();
	});

	it('uses coded login errors when the device returns an invalid access token', async () => {
		mockSignIn.mockResolvedValue({ access_token: '   ' });

		let loginState!: ReturnType<typeof useLogin>;
		const cleanup = $effect.root(() => {
			loginState = useLogin();
		});

		loginState.username = 'admin';
		loginState.password = 'secret';

		await loginState.signInUser({ username: 'admin', password: 'secret' });

		expect(mockErrorHandle).toHaveBeenCalledTimes(1);
		expect(mockErrorHandle.mock.calls[0]?.[0]).toMatchObject({
			message: 'auth/invalid_access_token'
		});
		expect(mockErrorHandle.mock.calls[0]?.[1]).toBe('Login failed.');
		expect(loginState.username).toBe('admin');
		expect(loginState.password).toBe('secret');
		expect(mockNotifications.error).not.toHaveBeenCalled();
		expect(mockOpenDefaultCredentialsWarning).not.toHaveBeenCalled();

		cleanup();
	});
});
