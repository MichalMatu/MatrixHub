import { beforeEach, describe, expect, it, vi } from 'vitest';

const { loggerError, mockModals } = vi.hoisted(() => ({
	loggerError: vi.fn(),
	mockModals: {
		open: vi.fn(),
		close: vi.fn()
	}
}));

vi.mock('svelte-modals', () => ({
	modals: mockModals
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: {}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: {
		success: vi.fn(),
		error: vi.fn()
	}
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn(
		(
			options: { modalService?: typeof mockModals; component?: unknown } & Record<string, unknown>
		) => {
			const { modalService = mockModals, component = {}, ...props } = options;
			return modalService.open(component, props);
		}
	)
}));

vi.mock('$lib/utils/ui/modal', () => ({
	openModal: vi.fn(
		(
			component: unknown,
			props?: Record<string, unknown>,
			modalService: typeof mockModals = mockModals,
			options?: Record<string, unknown>
		) => modalService.open(component as never, props as never, options as never)
	)
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn((error: unknown) =>
		typeof error === 'object' && error !== null && 'kind' in error
			? (error as { kind: string }).kind
			: null
	),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown';
	})
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: loggerError
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: {
		username: 'store-user',
		admin: true,
		bearer_token: 'store-token',
		invalidate: vi.fn()
	}
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	user_msg_updated: () => 'updated',
	toast_user_settings_load_timeout: () => 'load timeout',
	toast_user_settings_load_failed: () => 'load failed',
	toast_user_settings_save_timeout: () => 'save timeout',
	toast_user_settings_save_failed: () => 'save failed',
	user_dialog_delete_title: () => 'Delete user',
	user_dialog_delete_msg: ({ name }: { name: string }) => `delete ${name}`,
	action_abort: () => 'Abort',
	action_yes: () => 'Yes',
	user_dialog_edit_title: () => 'Edit user',
	user_dialog_add_title: () => 'Add user',
	toast_message: ({ message }: { message: string }) => message
}));

type TestUser = {
	username: string;
	password: string;
	admin: boolean;
};

type ModalPayload = Record<string, unknown> & {
	onConfirm?: () => void | Promise<void>;
	onSaveUser?: (user: TestUser) => void;
};

function flushPromises() {
	return new Promise((resolve) => setTimeout(resolve, 0));
}

describe('useUserManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('keeps local users unchanged when delete save fails', async () => {
		const { useUserManagement } = await import('./useUserManagement.svelte');

		const saveSecuritySettings = vi.fn().mockRejectedValue(new Error('save failed'));
		const modalService = {
			open: vi.fn(),
			close: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const management = useUserManagement({} as never, {
					createSecurityApi: () => ({
						getSecuritySettings: vi.fn().mockResolvedValue({
							jwt_secret: '',
							jwt_secret_configured: true,
							users: [{ username: 'alice', password: 'pwd', admin: true }]
						}),
						saveSecuritySettings
					}),
					modalService
				});

				void management.loadSettings().then(async () => {
					management.openDeleteConfirmation(0);

					const payload = modalService.open.mock.calls[0]?.[1] as ModalPayload;
					await payload.onConfirm?.();
					await flushPromises();

					expect(saveSecuritySettings).toHaveBeenCalledWith({
						jwt_secret: '',
						jwt_secret_configured: true,
						users: []
					});
					expect(management.state.securitySettings.users).toEqual([
						{ username: 'alice', password: 'pwd', admin: true }
					]);
					expect(management.state.error).toBe('save failed');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('invalidates current user when saved settings revoke authorization', async () => {
		const { useUserManagement } = await import('./useUserManagement.svelte');

		const successToast = vi.fn();
		const invalidateCurrentUser = vi.fn();

		let management: ReturnType<typeof useUserManagement>;

		const cleanup = $effect.root(() => {
			management = useUserManagement({} as never, {
				createSecurityApi: () => ({
					getSecuritySettings: vi.fn().mockResolvedValue({
						jwt_secret: '',
						jwt_secret_configured: true,
						users: [{ username: 'store-user', password: '', admin: true }]
					}),
					saveSecuritySettings: vi.fn().mockResolvedValue({
						jwt_secret: '',
						jwt_secret_configured: true,
						users: [{ username: 'store-user', password: '', admin: true }]
					})
				}),
				invalidateCurrentUser,
				notifications: {
					success: successToast,
					error: vi.fn()
				}
			});
		});

		await management!.loadSettings();
		management!.state.securitySettings.jwt_secret = 'new-secret';
		await management!.saveSettings(management!.state.securitySettings);

		expect(invalidateCurrentUser).toHaveBeenCalledTimes(1);
		expect(successToast).not.toHaveBeenCalled();

		cleanup();
	});

	it('uses injected secret generator and marks state dirty', async () => {
		const { useUserManagement } = await import('./useUserManagement.svelte');

		const cleanup = $effect.root(() => {
			const management = useUserManagement({} as never, {
				createSecurityApi: () => ({
					getSecuritySettings: vi.fn(),
					saveSecuritySettings: vi.fn()
				}),
				generateSecret: () => 'secret-123'
			});

			management.generateJwtSecret();

			expect(management.state.securitySettings.jwt_secret).toBe('secret-123');
			expect(management.state.securitySettings.jwt_secret_configured).toBe(true);
			expect(management.isDirty).toBe(true);
		});

		cleanup();
	});

	it('resets security draft to the last loaded settings', async () => {
		const { useUserManagement } = await import('./useUserManagement.svelte');

		let management: ReturnType<typeof useUserManagement>;

		const cleanup = $effect.root(() => {
			management = useUserManagement({} as never, {
				createSecurityApi: () => ({
					getSecuritySettings: vi.fn().mockResolvedValue({
						jwt_secret: 'saved-secret',
						jwt_secret_configured: true,
						users: [{ username: 'alice', password: 'pwd', admin: true }]
					}),
					saveSecuritySettings: vi.fn()
				})
			});
		});

		await management!.loadSettings();
		management!.state.securitySettings.jwt_secret = 'draft-secret';
		management!.state.securitySettings.users[0].admin = false;

		expect(management!.isDirty).toBe(true);

		management!.resetSettings();

		expect(management!.state.securitySettings).toEqual({
			jwt_secret: 'saved-secret',
			jwt_secret_configured: true,
			users: [{ username: 'alice', password: 'pwd', admin: true }]
		});
		expect(management!.isDirty).toBe(false);

		cleanup();
	});

	it('auto-loads security settings once per shouldLoad cycle', async () => {
		const { useUserManagement } = await import('./useUserManagement.svelte');
		const getSecuritySettings = vi.fn().mockResolvedValue({
			jwt_secret: '',
			jwt_secret_configured: true,
			users: [{ username: 'alice', password: 'pwd', admin: true }]
		});

		let cleanup: (() => void) | undefined;
		let setCanLoad!: (value: boolean) => void;

		cleanup = $effect.root(() => {
			let canLoad = $state(false);
			setCanLoad = (value: boolean) => {
				canLoad = value;
			};

			useUserManagement({} as never, {
				createSecurityApi: () => ({
					getSecuritySettings,
					saveSecuritySettings: vi.fn()
				}),
				shouldLoad: () => canLoad
			});
		});

		expect(getSecuritySettings).not.toHaveBeenCalled();

		setCanLoad(true);
		await vi.waitFor(() => {
			expect(getSecuritySettings).toHaveBeenCalledTimes(1);
		});

		await flushPromises();
		expect(getSecuritySettings).toHaveBeenCalledTimes(1);

		setCanLoad(false);
		await flushPromises();
		setCanLoad(true);

		await vi.waitFor(() => {
			expect(getSecuritySettings).toHaveBeenCalledTimes(2);
		});

		cleanup?.();
	});
});
