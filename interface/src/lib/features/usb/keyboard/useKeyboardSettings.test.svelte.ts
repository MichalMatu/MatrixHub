import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useKeyboardSettings } from './useKeyboardSettings.svelte';

const { mockConfirmRestartAndSave, mockNotifications, mockSession } = vi.hoisted(() => ({
	mockConfirmRestartAndSave: vi.fn(),
	mockNotifications: {
		error: vi.fn()
	},
	mockSession: {
		canManage: true,
		isAuthenticated: true
	}
}));

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => mockSession
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils/ui/restartConfirmation', () => ({
	confirmRestartAndSave: mockConfirmRestartAndSave
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'request failed';
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	restart_confirm_msg_generic: () => 'restart generic',
	request_error_failed: () => 'request failed',
	toast_message: ({ message }: { message: string }) => message
}));

describe('useKeyboardSettings', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockSession.canManage = true;
		mockSession.isAuthenticated = true;
	});

	it('loads keyboard config and mirrors it into local state', async () => {
		const api = {
			getConfig: vi.fn().mockResolvedValue({ enabled: true }),
			saveConfig: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const settings = useKeyboardSettings(() => api as never);

				void settings.fetchSettings().then(() => {
					expect(api.getConfig).toHaveBeenCalledOnce();
					expect(settings.savedConfig).toEqual({ enabled: true });
					expect(settings.localEnabled).toBe(true);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('uses restart confirmation directly from the toggle and saves the changed keyboard config', async () => {
		const api = {
			getConfig: vi.fn().mockResolvedValue({ enabled: false }),
			saveConfig: vi.fn().mockResolvedValue({ enabled: true })
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const settings = useKeyboardSettings(() => api as never);

				void settings.fetchSettings().then(async () => {
					settings.confirmEnabledChange(true);

					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
					const [onSave, options] = mockConfirmRestartAndSave.mock.calls[0];
					expect(options.message).toBe('restart generic');
					expect(options.triggerRestart).toBeUndefined();
					expect(settings.localEnabled).toBe(true);

					await onSave();

					expect(api.saveConfig).toHaveBeenCalledWith({ enabled: true });
					expect(settings.savedConfig).toEqual({ enabled: true });
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('restores the persisted value when restart confirmation is cancelled', async () => {
		const api = {
			getConfig: vi.fn().mockResolvedValue({ enabled: false }),
			saveConfig: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const settings = useKeyboardSettings(() => api as never);

				void settings.fetchSettings().then(() => {
					settings.confirmEnabledChange(true);

					expect(settings.localEnabled).toBe(true);
					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();

					const [, options] = mockConfirmRestartAndSave.mock.calls[0];
					options.onCancel?.();

					expect(settings.localEnabled).toBe(false);
					expect(api.saveConfig).not.toHaveBeenCalled();
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
