import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useMacroManagement } from './useMacroManagement.svelte';

const { mockConfirmRestartAndSave } = vi.hoisted(() => ({
	mockConfirmRestartAndSave: vi.fn()
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/utils/ui/restartConfirmation', () => ({
	confirmRestartAndSave: mockConfirmRestartAndSave
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	macros_boot_script_disabled: () => 'disabled',
	macros_error_script_content: () => 'script content error',
	macros_error_filename_required: () => 'filename required',
	macros_error_filename_invalid: () => 'filename invalid',
	macros_error_script_too_large: () => 'script too large',
	macros_error_save_script: () => 'save script error',
	macros_error_start: () => 'start error',
	macros_error_stop: () => 'stop error',
	macros_error_delete: () => 'delete error',
	settings_load_error: () => 'load error',
	settings_save_error: () => 'save settings error',
	toast_macro_script_saved: () => 'saved',
	macro_msg_saved: () => 'settings saved',
	restart_confirm_msg_generic: () => 'restart generic',
	macros_msg_started: () => 'started',
	macros_msg_stopped: () => 'stopped',
	macros_msg_deleted: () => 'deleted'
}));

describe('useMacroManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.spyOn(console, 'error').mockImplementation(() => undefined);
	});

	afterEach(() => {
		vi.restoreAllMocks();
	});

	it('uses restart confirmation when the macros enabled toggle changes', async () => {
		const notifications = {
			error: vi.fn(),
			success: vi.fn()
		};
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				boot_script: '',
				boot_delay: 5000
			}),
			listScripts: vi.fn().mockResolvedValue([]),
			getStatus: vi.fn(),
			getScriptContent: vi.fn(),
			uploadScript: vi.fn(),
			saveSettings: vi.fn().mockResolvedValue({
				enabled: false,
				boot_script: '',
				boot_delay: 5000
			}),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			deleteScript: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const macros = useMacroManagement({
					createApi: () => api,
					notifications
				});

				void macros.init().then(async () => {
					macros.localSettings.enabled = false;

					expect(macros.requiresSettingsRestart).toBe(true);

					macros.confirmSaveSettings();

					expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
					const [onSave, options] = mockConfirmRestartAndSave.mock.calls[0];
					expect(options.message).toBe('restart generic');

					await onSave();

					expect(api.saveSettings).toHaveBeenCalledWith({
						enabled: false,
						boot_script: '',
						boot_delay: 5000
					});
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('keeps only the newest edit request result', async () => {
		let resolveFirst: ((value: string) => void) | undefined;
		let resolveSecond: ((value: string) => void) | undefined;

		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				boot_script: '',
				boot_delay: 5000
			}),
			listScripts: vi.fn().mockResolvedValue([]),
			getStatus: vi.fn(),
			getScriptContent: vi
				.fn()
				.mockImplementationOnce(
					() =>
						new Promise<string>((resolve) => {
							resolveFirst = resolve;
						})
				)
				.mockImplementationOnce(
					() =>
						new Promise<string>((resolve) => {
							resolveSecond = resolve;
						})
				),
			uploadScript: vi.fn(),
			saveSettings: vi.fn(),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			deleteScript: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const macros = useMacroManagement({
					createApi: () => api,
					notifications: { error: vi.fn(), success: vi.fn() }
				});

				const firstRequest = macros.handleEdit('first.txt');
				const secondRequest = macros.handleEdit('second.txt');

				resolveSecond?.('second content');
				resolveFirst?.('first content');

				void Promise.all([firstRequest, secondRequest]).then(() => {
					expect(macros.editFilename).toBe('second.txt');
					expect(macros.editContent).toBe('second content');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('does not close the editor when script save returns ok=false', async () => {
		const notifications = {
			error: vi.fn(),
			success: vi.fn()
		};
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				boot_script: '',
				boot_delay: 5000
			}),
			listScripts: vi.fn().mockResolvedValue([]),
			getStatus: vi.fn(),
			getScriptContent: vi.fn(),
			uploadScript: vi.fn().mockResolvedValue({
				ok: false,
				error: 'backend rejected'
			}),
			saveSettings: vi.fn(),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			deleteScript: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const macros = useMacroManagement({
					createApi: () => api,
					notifications
				});

				macros.handleNew();

				void macros
					.saveScript({
						filename: 'test.txt',
						content: 'STRING test'
					})
					.then(() => {
						expect(macros.showEditor).toBe(true);
						expect(notifications.error).toHaveBeenCalledWith('backend rejected', 3000);
						expect(notifications.success).not.toHaveBeenCalled();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('rejects unsafe filenames before uploading', async () => {
		const notifications = {
			error: vi.fn(),
			success: vi.fn()
		};
		const api = {
			getSettings: vi.fn(),
			listScripts: vi.fn(),
			getStatus: vi.fn(),
			getScriptContent: vi.fn(),
			uploadScript: vi.fn(),
			saveSettings: vi.fn(),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			deleteScript: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const macros = useMacroManagement({
					createApi: () => api,
					notifications
				});

				void macros
					.saveScript({
						filename: '../bad.txt',
						content: 'REM test'
					})
					.then(() => {
						expect(api.uploadScript).not.toHaveBeenCalled();
						expect(notifications.error).toHaveBeenCalledWith('filename invalid', 3000);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('rejects oversized scripts before uploading', async () => {
		const notifications = {
			error: vi.fn(),
			success: vi.fn()
		};
		const api = {
			getSettings: vi.fn(),
			listScripts: vi.fn(),
			getStatus: vi.fn(),
			getScriptContent: vi.fn(),
			uploadScript: vi.fn(),
			saveSettings: vi.fn(),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			deleteScript: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const macros = useMacroManagement({
					createApi: () => api,
					notifications
				});

				void macros
					.saveScript({
						filename: 'large.txt',
						content: 'A'.repeat(8193)
					})
					.then(() => {
						expect(api.uploadScript).not.toHaveBeenCalled();
						expect(notifications.error).toHaveBeenCalledWith('script too large', 3000);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('syncs settings from the canonical save response', async () => {
		const notifications = {
			error: vi.fn(),
			success: vi.fn()
		};
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				boot_script: '',
				boot_delay: 5000
			}),
			listScripts: vi.fn().mockResolvedValue([{ name: 'boot.txt' }]),
			getStatus: vi.fn(),
			getScriptContent: vi.fn(),
			uploadScript: vi.fn(),
			saveSettings: vi.fn().mockResolvedValue({
				enabled: true,
				boot_script: 'boot.txt',
				boot_delay: 6000
			}),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			deleteScript: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const macros = useMacroManagement({
					createApi: () => api,
					notifications
				});

				void macros.init().then(async () => {
					macros.localSettings.boot_script = 'boot.txt';
					macros.localSettings.boot_delay = 6000;

					await macros.saveSettings();

					expect(api.saveSettings).toHaveBeenCalledWith({
						enabled: true,
						boot_script: 'boot.txt',
						boot_delay: 6000
					});
					expect(macros.settings).toEqual({
						enabled: true,
						boot_script: 'boot.txt',
						boot_delay: 6000
					});
					expect(macros.localSettings).toEqual({
						enabled: true,
						boot_script: 'boot.txt',
						boot_delay: 6000
					});
					expect(notifications.success).toHaveBeenCalledWith('settings saved', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
