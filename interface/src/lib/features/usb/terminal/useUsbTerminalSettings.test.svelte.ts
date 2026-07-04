import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockConfirmRestartAndSave, mockNotifications } = vi.hoisted(() => ({
	mockConfirmRestartAndSave: vi.fn(),
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn(),
		warning: vi.fn()
	}
}));

vi.mock('$lib/utils/ui/restartConfirmation', () => ({
	confirmRestartAndSave: mockConfirmRestartAndSave
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
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

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: vi.fn()
	})
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	settings_load_error: () => 'load error',
	settings_save_error: () => 'save error',
	settings_saved: () => 'saved',
	settings_validation_error: () => 'validation error',
	restart_confirm_msg_generic: () => 'restart generic',
	usb_terminal_disable_requires_cancel: () =>
		'Stop the active terminal session before disabling USB Terminal.',
	toast_message: ({ message }: { message: string }) => message
}));

describe('useUsbTerminalSettings', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	function flushPromises() {
		return new Promise((resolve) => setTimeout(resolve, 0));
	}

	it('sanitizes loaded config and keeps clean baselines for both drafts', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: true,
							idle_timeout_ms: 45000,
							target_port: '/dev/ttyUSB0\u0007'
						}),
						updateConfig: vi.fn()
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(() => {
					expect(terminal.settings).toEqual({
						enabled: true,
						idle_timeout_ms: 30000,
						target_port: '/dev/ttyUSB0'
					});
					expect(terminal.hasConfig).toBe(true);
					expect(terminal.hasEnabledChanges).toBe(false);
					expect(terminal.hasAdvancedChanges).toBe(false);
					expect(terminal.hasChanges).toBe(false);
					expect(terminal.error).toBeNull();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('auto-loads config once per shouldLoad cycle', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');
		const getConfig = vi.fn().mockResolvedValue({
			enabled: true,
			idle_timeout_ms: 2000,
			target_port: '/dev/ttyUSB0'
		});

		let cleanup: (() => void) | undefined;
		let setCanLoad!: (value: boolean) => void;

		cleanup = $effect.root(() => {
			let canLoad = $state(false);
			setCanLoad = (value: boolean) => {
				canLoad = value;
			};

			useUsbTerminalSettings({
				createApi: () => ({
					getConfig,
					updateConfig: vi.fn()
				}),
				notifications: mockNotifications,
				shouldLoad: () => canLoad
			});
		});

		expect(getConfig).not.toHaveBeenCalled();

		setCanLoad(true);
		await vi.waitFor(() => {
			expect(getConfig).toHaveBeenCalledTimes(1);
		});

		await flushPromises();
		expect(getConfig).toHaveBeenCalledTimes(1);

		setCanLoad(false);
		await flushPromises();
		setCanLoad(true);

		await vi.waitFor(() => {
			expect(getConfig).toHaveBeenCalledTimes(2);
		});

		cleanup?.();
	});

	it('saves only the enabled toggle from the card and keeps advanced draft dirty', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		const updateConfig = vi.fn().mockResolvedValue({
			enabled: true,
			idle_timeout_ms: 2000,
			target_port: '/dev/ttyUSB0'
		});
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: false,
							idle_timeout_ms: 2000,
							target_port: '/dev/ttyUSB0'
						}),
						updateConfig
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(async () => {
					terminal.setEnabled(true);
					terminal.updateTargetPort('/dev/ttyUSB9');

					await terminal.saveEnabled();

					expect(updateConfig).toHaveBeenCalledWith({
						enabled: true,
						idle_timeout_ms: 2000,
						target_port: '/dev/ttyUSB0'
					});
					expect(terminal.enabled).toBe(true);
					expect(terminal.advancedSettings.target_port).toBe('/dev/ttyUSB9');
					expect(terminal.hasEnabledChanges).toBe(false);
					expect(terminal.hasAdvancedChanges).toBe(true);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('uses restart confirmation directly from the enabled toggle', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		const updateConfig = vi.fn().mockResolvedValue({
			enabled: true,
			idle_timeout_ms: 2000,
			target_port: '/dev/ttyUSB0'
		});
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: false,
							idle_timeout_ms: 2000,
							target_port: '/dev/ttyUSB0'
						}),
						updateConfig
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(async () => {
					terminal.confirmEnabledChange(true);

					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
					const [onSave, options] = mockConfirmRestartAndSave.mock.calls[0];
					expect(options.message).toBe('restart generic');
					expect(terminal.enabled).toBe(true);

					await onSave();

					expect(updateConfig).toHaveBeenCalledWith({
						enabled: true,
						idle_timeout_ms: 2000,
						target_port: '/dev/ttyUSB0'
					});
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('restores the persisted enabled state when the restart confirmation is cancelled', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: false,
							idle_timeout_ms: 2000,
							target_port: '/dev/ttyUSB0'
						}),
						updateConfig: vi.fn()
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(() => {
					terminal.confirmEnabledChange(true);

					expect(terminal.enabled).toBe(true);
					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();

					const [, options] = mockConfirmRestartAndSave.mock.calls[0];
					options.onCancel?.();

					expect(terminal.enabled).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('saves only advanced settings from the modal and keeps inline enabled draft dirty', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		const updateConfig = vi.fn().mockResolvedValue({
			enabled: false,
			idle_timeout_ms: 30000,
			target_port: '/dev/ttyUSB1'
		});
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: false,
							idle_timeout_ms: 2000,
							target_port: '/dev/ttyUSB0'
						}),
						updateConfig
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(async () => {
					terminal.setEnabled(true);
					terminal.updateTargetPort('/dev/ttyUSB1\u0000');
					terminal.updateIdleTimeout('45000');

					await terminal.saveAdvancedSettings();

					expect(updateConfig).toHaveBeenCalledWith({
						enabled: false,
						idle_timeout_ms: 30000,
						target_port: '/dev/ttyUSB1'
					});
					expect(terminal.enabled).toBe(true);
					expect(terminal.advancedSettings).toEqual({
						idle_timeout_ms: 30000,
						target_port: '/dev/ttyUSB1'
					});
					expect(terminal.hasEnabledChanges).toBe(true);
					expect(terminal.hasAdvancedChanges).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('resets advanced draft back to saved settings when modal changes are discarded', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: true,
							idle_timeout_ms: 2000,
							target_port: '/dev/ttyUSB0'
						}),
						updateConfig: vi.fn()
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(() => {
					terminal.updateTargetPort('/dev/ttyUSB2');
					terminal.updateIdleTimeout('9999');

					expect(terminal.hasAdvancedChanges).toBe(true);

					terminal.resetAdvancedDraft();

					expect(terminal.advancedSettings).toEqual({
						idle_timeout_ms: 2000,
						target_port: '/dev/ttyUSB0'
					});
					expect(terminal.hasAdvancedChanges).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('normalizes missing target port to auto when enabling terminal from the card', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		const updateConfig = vi.fn().mockResolvedValue({
			enabled: true,
			idle_timeout_ms: 2000,
			target_port: 'auto'
		});
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: false,
							idle_timeout_ms: 2000,
							target_port: ''
						}),
						updateConfig
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(async () => {
					expect(terminal.advancedSettings.target_port).toBe('auto');

					terminal.setEnabled(true);

					await terminal.saveEnabled();

					expect(updateConfig).toHaveBeenCalledWith({
						enabled: true,
						idle_timeout_ms: 2000,
						target_port: 'auto'
					});
					expect(terminal.errors.target_port).toBe(false);
					expect(mockNotifications.warning).not.toHaveBeenCalled();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('maps active session conflict to a localized save error message', async () => {
		const { useUsbTerminalSettings } = await import('./useUsbTerminalSettings.svelte');

		const updateConfig = vi.fn().mockRejectedValue(new Error('usb_terminal/session_active'));
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const terminal = useUsbTerminalSettings({
					createApi: () => ({
						getConfig: vi.fn().mockResolvedValue({
							enabled: true,
							idle_timeout_ms: 2000,
							target_port: '/dev/ttyUSB0'
						}),
						updateConfig
					}),
					notifications: mockNotifications
				});

				void terminal.loadSettings().then(async () => {
					terminal.setEnabled(false);

					await terminal.saveEnabled();

					expect(updateConfig).toHaveBeenCalledWith({
						enabled: false,
						idle_timeout_ms: 2000,
						target_port: '/dev/ttyUSB0'
					});
					expect(terminal.hasEnabledChanges).toBe(true);
					expect(mockNotifications.error).toHaveBeenCalledWith(
						'Stop the active terminal session before disabling USB Terminal.',
						3000
					);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
