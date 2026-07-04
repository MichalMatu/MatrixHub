import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';

const { mockSystemStatus } = vi.hoisted(() => {
	const systemStatus = {
		macros: null as ScriptStatus | null,
		subscribeChannel: vi.fn(),
		unsubscribeChannel: vi.fn(),
		setMacros: vi.fn((status: ScriptStatus | null) => {
			systemStatus.macros = status;
		})
	};

	return {
		mockSystemStatus: systemStatus
	};
});

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: mockSystemStatus
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
	usb_terminal_quick_scripts_load_error: () => 'Failed to load quick scripts.',
	usb_terminal_quick_scripts_run_error: () => 'Failed to start the selected script.',
	usb_terminal_quick_scripts_stop_error: () => 'Failed to stop the running script.'
}));

describe('useUsbTerminalQuickScripts', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockSystemStatus.macros = null;
	});

	afterEach(() => {
		vi.restoreAllMocks();
	});

	it('loads scripts, settings, and macro status on init', async () => {
		const idleStatus: ScriptStatus = {
			current_script: '',
			status: 'IDLE',
			current_line: 0,
			uptime_ms: 0,
			last_error: ''
		};

		const { useUsbTerminalQuickScripts } = await import('./useUsbTerminalQuickScripts.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const quickScripts = useUsbTerminalQuickScripts({
					channelStore: mockSystemStatus,
					createApi: () => ({
						listScripts: vi.fn().mockResolvedValue([{ name: 'zeta.sh' }, { name: 'alpha.sh' }]),
						getSettings: vi.fn().mockResolvedValue({
							enabled: false,
							boot_script: '',
							boot_delay: 1000
						}),
						getStatus: vi.fn().mockResolvedValue(idleStatus),
						runScript: vi.fn(),
						stopScript: vi.fn()
					})
				});

				void quickScripts.init().then(() => {
					expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('macros');
					expect(mockSystemStatus.setMacros).toHaveBeenCalledWith(idleStatus);
					expect(quickScripts.scripts.map((script) => script.name)).toEqual([
						'alpha.sh',
						'zeta.sh'
					]);
					expect(quickScripts.macrosEnabled).toBe(false);
					expect(quickScripts.shouldShowSection).toBe(true);
					expect(quickScripts.error).toBeNull();
					quickScripts.destroy();
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('runs and stops scripts using existing HTTP endpoints while syncing websocket status', async () => {
		const idleStatus: ScriptStatus = {
			current_script: '',
			status: 'IDLE',
			current_line: 0,
			uptime_ms: 0,
			last_error: ''
		};
		const runningStatus: ScriptStatus = {
			current_script: 'detect.sh',
			status: 'RUNNING',
			current_line: 3,
			uptime_ms: 250,
			last_error: ''
		};

		const getStatus = vi
			.fn()
			.mockResolvedValueOnce(idleStatus)
			.mockResolvedValueOnce(runningStatus)
			.mockResolvedValueOnce(idleStatus);
		const runScript = vi.fn().mockResolvedValue({ ok: true, status: 'started' });
		const stopScript = vi.fn().mockResolvedValue({ ok: true, status: 'stopped' });

		const { useUsbTerminalQuickScripts } = await import('./useUsbTerminalQuickScripts.svelte');

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const quickScripts = useUsbTerminalQuickScripts({
					channelStore: mockSystemStatus,
					createApi: () => ({
						listScripts: vi.fn().mockResolvedValue([{ name: 'detect.sh' }]),
						getSettings: vi.fn().mockResolvedValue({
							enabled: true,
							boot_script: '',
							boot_delay: 1000
						}),
						getStatus,
						runScript,
						stopScript
					})
				});

				void quickScripts.init().then(async () => {
					expect(quickScripts.isTerminalCommandDisabled).toBe(false);

					await quickScripts.runScript('detect.sh');

					expect(runScript).toHaveBeenCalledWith('detect.sh');
					expect(quickScripts.runningScriptName).toBe('detect.sh');
					expect(quickScripts.isRunningScript('detect.sh')).toBe(true);
					expect(quickScripts.isTerminalCommandDisabled).toBe(true);
					expect(quickScripts.isScriptDisabled('other.sh', false)).toBe(true);

					await quickScripts.stopScript();

					expect(stopScript).toHaveBeenCalled();
					expect(quickScripts.runningScriptName).toBe('');
					expect(quickScripts.isTerminalCommandDisabled).toBe(false);
					quickScripts.destroy();
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
