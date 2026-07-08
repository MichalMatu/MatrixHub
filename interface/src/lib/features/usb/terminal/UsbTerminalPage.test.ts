import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

const { mockPage, mockUser, mockTerminalState, mockConsoleState, mockQuickScriptsState } =
	vi.hoisted(() => ({
		mockPage: {
			data: {
				features: {
					security: true
				}
			}
		},
		mockUser: {
			admin: true,
			bearer_token: 'token',
			isValid: true
		},
		mockTerminalState: {
			loading: false,
			hasConfig: true,
			error: null,
			saving: false,
			enabled: true,
			hasEnabledChanges: false,
			advancedSettings: {
				target_port: '/dev/ttyUSB0',
				idle_timeout_ms: 2000
			},
			errors: {
				target_port: false,
				idle_timeout_ms: false
			},
			loadSettings: vi.fn(),
			resetAdvancedDraft: vi.fn(),
			saveAdvancedSettings: vi.fn().mockResolvedValue(true),
			saveEnabled: vi.fn().mockResolvedValue(true),
			confirmEnabledChange: vi.fn(),
			setEnabled: vi.fn(),
			updateTargetPort: vi.fn(),
			updateIdleTimeout: vi.fn(),
			normalizeIdleTimeout: vi.fn()
		},
		mockConsoleState: {
			isConnected: true,
			busy: false,
			owner: false,
			transport: null as 'telegram' | 'ws' | null,
			currentDirectory: null as string | null,
			currentPrompt: '$',
			command: '',
			entries: [] as Array<{ id: number; phase: string; text: string }>,
			error: null,
			notice: null,
			canExecute: true,
			canCancel: false,
			init: vi.fn(),
			destroy: vi.fn(),
			updateCommand: vi.fn(),
			sendCommand: vi.fn(),
			sendDiagnosticCommand: vi.fn(),
			sendCancel: vi.fn(),
			clearEntries: vi.fn()
		},
		mockQuickScriptsState: {
			loading: false,
			hostActions: [{ id: 'macos-focus-terminal', name: 'Focus macOS Terminal' }],
			scripts: [{ name: 'Detect ESP' }, { name: 'Find Port' }],
			macrosEnabled: true,
			error: null,
			pendingScriptName: null,
			pendingHostActionId: null,
			pendingAction: null,
			isTerminalCommandDisabled: false,
			shouldShowSection: true,
			init: vi.fn(),
			destroy: vi.fn(),
			runHostAction: vi.fn(),
			runScript: vi.fn(),
			stopScript: vi.fn(),
			isHostActionDisabled: vi.fn(() => false),
			isRunningScript: vi.fn(() => false),
			isScriptDisabled: vi.fn(() => false)
		}
	}));

vi.mock('$app/state', () => ({
	page: mockPage
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	usb_terminal_title: () => 'USB Terminal',
	usb_terminal_enable: () => 'Enable USB terminal',
	usb_terminal_open_settings: () => 'Terminal settings',
	usb_terminal_security_required: () => 'Security required',
	usb_terminal_status_disabled: () => 'Disabled',
	usb_terminal_status_idle: () => 'Idle',
	usb_terminal_status_busy_web: () => 'Busy (web)',
	usb_terminal_status_busy_telegram: () => 'Busy (Telegram)',
	usb_terminal_empty: () => 'No terminal output yet.',
	usb_terminal_focus_warning: () => 'Before sending, focus a real terminal window on the host.',
	usb_terminal_command_presets_title: () => 'Command Presets',
	usb_terminal_command_presets_hint: () => 'Selecting a preset fills Command without sending it.',
	usb_terminal_preset_os_label: () => 'Host System',
	usb_terminal_preset_command_label: () => 'Prepared Command',
	usb_terminal_preset_placeholder: () => 'Choose command',
	usb_terminal_command_label: () => 'Command',
	usb_terminal_command_placeholder: () => 'uname -a',
	usb_terminal_stop: () => 'Stop',
	usb_terminal_run_id: () => 'Run id',
	usb_terminal_quick_scripts_title: () => 'Quick Scripts',
	usb_terminal_quick_scripts_requires_macros: () =>
		'Running quick scripts requires enabled Macros.',
	usb_terminal_quick_scripts_open_macros: () => 'Open Macros',
	usb_terminal_quick_scripts_loading: () => 'Loading quick scripts...',
	usb_terminal_quick_scripts_empty: () => 'No saved scripts available.',
	usb_terminal_disable_requires_cancel: () =>
		'Stop the active terminal session before disabling USB Terminal.',
	usb_terminal_target_port: () => 'Target port',
	usb_terminal_target_port_placeholder: () => 'auto',
	usb_terminal_target_port_auto: () => 'Auto',
	usb_terminal_target_port_desc: () => 'Path to USB serial device.',
	usb_terminal_idle_timeout: () => 'Idle timeout',
	usb_terminal_idle_timeout_desc: () => 'Wait before command output is considered complete.',
	action_save: () => 'Save',
	action_clear: () => 'Clear',
	action_cancel: () => 'Cancel',
	action_close: () => 'Close',
	keyboard_send: () => 'Send',
	status_online: () => 'Online',
	status_offline: () => 'Offline',
	settings_load_error: () => 'Failed to load settings',
	settings_validation_error: () => 'Validation error'
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('./useUsbTerminalSettings.svelte', () => ({
	useUsbTerminalSettings: () => mockTerminalState
}));

vi.mock('./useUsbTerminalConsole.svelte', () => ({
	useUsbTerminalConsole: () => mockConsoleState
}));

vi.mock('./useUsbTerminalQuickScripts.svelte', () => ({
	useUsbTerminalQuickScripts: () => mockQuickScriptsState
}));

describe('UsbTerminalPage', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockTerminalState.error = null;
		mockTerminalState.saving = false;
		mockTerminalState.enabled = true;
		mockTerminalState.hasEnabledChanges = false;
		mockConsoleState.busy = false;
		mockConsoleState.owner = false;
		mockConsoleState.transport = null;
		mockConsoleState.currentDirectory = null;
		mockConsoleState.currentPrompt = '$';
		mockConsoleState.command = '';
		mockConsoleState.entries = [];
		mockConsoleState.error = null;
		mockConsoleState.notice = null;
		mockQuickScriptsState.loading = false;
		mockQuickScriptsState.hostActions = [
			{ id: 'macos-focus-terminal', name: 'Focus macOS Terminal' }
		];
		mockQuickScriptsState.scripts = [{ name: 'Detect ESP' }, { name: 'Find Port' }];
		mockQuickScriptsState.macrosEnabled = true;
		mockQuickScriptsState.error = null;
		mockQuickScriptsState.pendingScriptName = null;
		mockQuickScriptsState.pendingHostActionId = null;
		mockQuickScriptsState.pendingAction = null;
		mockQuickScriptsState.isTerminalCommandDisabled = false;
		mockQuickScriptsState.shouldShowSection = true;
		mockQuickScriptsState.runHostAction = vi.fn();
		mockQuickScriptsState.isHostActionDisabled = vi.fn(() => false);
		mockQuickScriptsState.isRunningScript = vi.fn(() => false);
		mockQuickScriptsState.isScriptDisabled = vi.fn(() => false);
		Object.defineProperty(HTMLElement.prototype, 'scrollTo', {
			value: vi.fn(),
			configurable: true
		});
		Object.defineProperty(HTMLElement.prototype, 'animate', {
			value: vi.fn(() => ({
				cancel: vi.fn(),
				finished: Promise.resolve(),
				onfinish: null
			})),
			configurable: true
		});
	});

	afterEach(() => {
		vi.restoreAllMocks();
		cleanup();
	});

	it('renders a single card with top actions, badges, and opens the settings modal', async () => {
		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		expect(screen.getByText('USB Terminal')).toBeTruthy();
		expect(screen.getByText('Online')).toBeTruthy();
		expect(screen.getByText('Idle')).toBeTruthy();
		expect(screen.getByTestId('usb-terminal-focus-warning').textContent).toContain(
			'Before sending, focus a real terminal window on the host.'
		);
		expect(screen.getByTestId('usb-terminal-prompt').textContent).toContain('$');
		expect(screen.queryByText('Terminal is idle.')).toBeNull();
		expect(screen.getByText('Command Presets')).toBeTruthy();
		expect(screen.getByRole('combobox', { name: 'Host System' })).toBeTruthy();
		expect(screen.getByRole('combobox', { name: 'Prepared Command' })).toBeTruthy();
		expect(screen.getByText('Quick Scripts')).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Focus macOS Terminal' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Detect ESP' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Find Port' })).toBeTruthy();
		expect(screen.queryByRole('button', { name: 'Save' })).toBeNull();

		expect(screen.getAllByRole('button', { name: 'Clear' })).toHaveLength(1);
		expect(screen.getByRole('button', { name: 'Stop' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Run id' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Send' })).toBeTruthy();

		await fireEvent.click(screen.getByRole('checkbox', { name: 'Enable USB terminal' }));

		expect(mockTerminalState.confirmEnabledChange).toHaveBeenCalledWith(false);

		await fireEvent.click(screen.getByRole('button', { name: 'Terminal settings' }));

		expect(mockTerminalState.resetAdvancedDraft).toHaveBeenCalled();
		expect(await screen.findByLabelText('Target port', {}, { timeout: 5000 })).toBeTruthy();
		expect(await screen.findByLabelText('Idle timeout', {}, { timeout: 5000 })).toBeTruthy();
	}, 20000);

	it('fills the command input from an OS-specific preset without sending it', async () => {
		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		await fireEvent.change(screen.getByRole('combobox', { name: 'Host System' }), {
			target: { value: 'linux' }
		});
		await fireEvent.change(screen.getByRole('combobox', { name: 'Prepared Command' }), {
			target: { value: 'linux-smoke' }
		});

		expect(mockConsoleState.updateCommand).toHaveBeenCalledWith(
			"printf 'MATRIXHUB_LINUX_OK\\n'; uname -a; pwd"
		);
		expect(mockConsoleState.sendCommand).not.toHaveBeenCalled();
		expect(screen.getByTestId('usb-terminal-preset-description').textContent).toContain(
			'Prints a known marker'
		);
	});

	it('runs the id diagnostic command from the toolbar', async () => {
		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		await fireEvent.click(screen.getByRole('button', { name: 'Run id' }));

		expect(mockConsoleState.sendDiagnosticCommand).toHaveBeenCalledTimes(1);
	});

	it('runs the built-in host focus quick action', async () => {
		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		await fireEvent.click(screen.getByRole('button', { name: 'Focus macOS Terminal' }));

		expect(mockQuickScriptsState.runHostAction).toHaveBeenCalledWith('macos-focus-terminal');
		expect(mockConsoleState.sendCommand).not.toHaveBeenCalled();
	});

	it('shows macros disabled notice and disables quick scripts without blocking manual terminal input', async () => {
		mockQuickScriptsState.macrosEnabled = false;
		mockQuickScriptsState.scripts = [{ name: 'Detect ESP' }];
		mockQuickScriptsState.isScriptDisabled = vi.fn(() => true);

		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		expect(screen.getAllByText('Running quick scripts requires enabled Macros.')).toHaveLength(1);
		expect(screen.getByRole('link', { name: 'Open Macros' }).getAttribute('href')).toBe(
			'/usb-features/macros'
		);
		expect(screen.getByRole('button', { name: 'Detect ESP' }).hasAttribute('disabled')).toBe(true);
		expect(screen.getByRole('button', { name: 'Send' }).hasAttribute('disabled')).toBe(false);
	});

	it('renders prompt and output blocks without break-all wrapping', async () => {
		mockConsoleState.currentDirectory = '/tmp';
		mockConsoleState.currentPrompt = '/tmp $';
		mockConsoleState.entries = [
			{
				id: 1,
				phase: 'final',
				text: 'alpha   beta   gamma'
			}
		];

		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		expect(screen.getByTestId('usb-terminal-prompt').textContent).toContain('/tmp $');
		const outputBlock = screen.getByTestId('usb-terminal-entry').querySelector('pre');
		expect(outputBlock?.className.includes('break-all')).toBe(false);
		expect(outputBlock?.className.includes('whitespace-pre-wrap')).toBe(true);
	});

	it('shows the active-session warning for a pending disable state', async () => {
		mockConsoleState.busy = true;
		mockConsoleState.transport = 'telegram';
		mockTerminalState.enabled = false;
		mockTerminalState.hasEnabledChanges = true;

		const { default: UsbTerminalPage } = await import('./UsbTerminalPage.svelte');
		render(UsbTerminalPage);

		expect(
			screen.getByText('Stop the active terminal session before disabling USB Terminal.')
		).toBeTruthy();
	});
});
