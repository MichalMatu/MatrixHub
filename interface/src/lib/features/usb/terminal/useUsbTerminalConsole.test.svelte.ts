import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useUsbTerminalConsole } from './useUsbTerminalConsole.svelte';

const { mockPage, mockUser, mockNotifications } = vi.hoisted(() => ({
	mockPage: {
		data: {
			features: { security: true }
		}
	},
	mockUser: {
		admin: true,
		bearer_token: 'token',
		isValid: true
	},
	mockNotifications: {
		info: vi.fn(),
		success: vi.fn(),
		warning: vi.fn(),
		error: vi.fn()
	}
}));

vi.mock('$app/state', () => ({
	page: mockPage
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	usb_terminal_console_requires_security: () =>
		'USB terminal console requires security to be enabled.',
	usb_terminal_parse_error: () => 'Failed to parse USB terminal response.',
	usb_terminal_request_failed: () => 'Terminal request failed.',
	usb_terminal_connection_error: () => 'USB terminal connection error.',
	usb_terminal_connection_established: () => 'Terminal connection established.',
	usb_terminal_connection_lost_reconnecting: () => 'USB terminal connection lost. Reconnecting...',
	usb_terminal_not_connected: () => 'USB terminal is not connected.',
	usb_terminal_error_generic: () => 'USB terminal error.',
	usb_terminal_message_disabled_service: () =>
		'USB Terminal Service is disabled. Enable it in the Web UI.',
	usb_terminal_message_busy_other_session: () => 'Terminal is busy with another session.',
	usb_terminal_message_busy_own_session: () =>
		'Terminal is busy processing the previous command. Use status or cancel.',
	usb_terminal_message_internal_buffer: () =>
		'Internal error: failed to allocate terminal output buffer.',
	usb_terminal_message_internal_command: () => 'Internal error: failed to type the command.',
	usb_terminal_message_port_not_set: () => 'Port not set. Typing blindly without feedback.',
	usb_terminal_message_idle: () => 'Terminal is idle.',
	usb_terminal_message_running: () => 'Command is still running.',
	usb_terminal_message_running_no_output: () =>
		'Command is running, but no output has been captured yet.',
	usb_terminal_message_interrupted: () => 'Interrupt signal (Ctrl+C) sent to host.',
	usb_terminal_message_service_unavailable: () => 'USB terminal service is unavailable.',
	usb_terminal_message_invalid_request: () => 'Invalid terminal request payload.',
	usb_terminal_message_unsupported_request: () => 'Unsupported terminal request type.'
}));

class MockWebSocket {
	static CONNECTING = 0;
	static OPEN = 1;
	static CLOSING = 2;
	static CLOSED = 3;
	static instances: MockWebSocket[] = [];

	onopen: (() => void) | null = null;
	onclose: (() => void) | null = null;
	onmessage: ((event: MessageEvent) => void) | null = null;
	onerror: (() => void) | null = null;
	readyState = MockWebSocket.CONNECTING;
	send = vi.fn();
	close = vi.fn(() => {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.();
	});

	constructor(public url: string) {
		MockWebSocket.instances.push(this);
	}

	emitOpen() {
		this.readyState = MockWebSocket.OPEN;
		this.onopen?.();
	}

	emitClose() {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.();
	}

	emitMessage(payload: unknown) {
		this.onmessage?.({
			data: typeof payload === 'string' ? payload : JSON.stringify(payload)
		} as MessageEvent);
	}
}

describe('useUsbTerminalConsole', () => {
	beforeEach(() => {
		MockWebSocket.instances = [];
		vi.useFakeTimers();
		vi.clearAllMocks();
		vi.stubGlobal('WebSocket', MockWebSocket);
		Object.defineProperty(document, 'hidden', {
			value: false,
			configurable: true
		});
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.unstubAllGlobals();
		vi.useRealTimers();
	});

	it('connects to the dedicated USB terminal websocket and sends commands', () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			expect(socket.url).toContain('/ws/usbterminal');
			expect(socket.url).not.toContain('access_token=');

			socket.emitOpen();
			socket.emitMessage({
				type: 'session',
				busy: false,
				owner: false,
				transport: null,
				connected: true
			});
			expect(mockNotifications.info).toHaveBeenCalledWith('Terminal connection established.', 5000);

			consoleState.updateCommand('  ls -la  ');
			expect(consoleState.sendCommand()).toBe(true);
			expect(socket.send).toHaveBeenCalledWith(
				JSON.stringify({ type: 'execute', command: 'ls -la' })
			);
			expect(consoleState.command).toBe('');
			expect(consoleState.entries[0]).toMatchObject({
				phase: 'command',
				text: '$ ls -la'
			});
		});

		consoleState.destroy();
		cleanup?.();
	});

	it('updates session ownership and appends output from websocket frames', () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			socket.emitOpen();

			socket.emitMessage({
				type: 'session',
				busy: true,
				owner: true,
				transport: 'ws',
				connected: true
			});
			socket.emitMessage({
				type: 'output',
				phase: 'partial',
				text: 'hello from host'
			});
			socket.emitMessage({
				type: 'ack',
				action: 'status',
				ok: true,
				message: 'Command is still running.'
			});

			expect(consoleState.busy).toBe(true);
			expect(consoleState.owner).toBe(true);
			expect(consoleState.transport).toBe('ws');
			expect(mockNotifications.info).toHaveBeenLastCalledWith('Command is still running.', 5000);
			expect(consoleState.canExecute).toBe(false);
			expect(consoleState.canCancel).toBe(true);
			expect(consoleState.entries[0]).toMatchObject({
				phase: 'partial',
				text: 'hello from host'
			});
		});

		consoleState.destroy();
		cleanup?.();
	});

	it('merges streamed partial and final output into a single block', () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			socket.emitOpen();
			socket.emitMessage({
				type: 'session',
				busy: true,
				owner: true,
				transport: 'ws',
				connected: true
			});
			socket.emitMessage({
				type: 'output',
				phase: 'partial',
				text: 'hello '
			});
			socket.emitMessage({
				type: 'output',
				phase: 'final',
				text: 'world'
			});

			expect(consoleState.entries).toHaveLength(1);
			expect(consoleState.entries[0]).toMatchObject({
				phase: 'final',
				text: 'hello world'
			});
		});

		consoleState.destroy();
		cleanup?.();
	});

	it('tracks current directory from pwd output and uses it in the prompt', () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			socket.emitOpen();
			socket.emitMessage({
				type: 'session',
				busy: false,
				owner: false,
				transport: null,
				connected: true
			});

			consoleState.updateCommand('pwd');
			expect(consoleState.sendCommand()).toBe(true);
			socket.emitMessage({
				type: 'output',
				phase: 'final',
				text: '/tmp/project'
			});
			socket.emitMessage({
				type: 'session',
				busy: false,
				owner: false,
				transport: null,
				connected: true
			});

			expect(consoleState.currentDirectory).toBe('/tmp/project');
			expect(consoleState.currentPrompt).toBe('/tmp/project $');

			consoleState.updateCommand('ls');
			expect(consoleState.sendCommand()).toBe(true);
			expect(consoleState.entries.at(-1)).toMatchObject({
				phase: 'command',
				text: '/tmp/project $ ls'
			});
		});

		consoleState.destroy();
		cleanup?.();
	});

	it('clears the prompt when another transport owns the active session', () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			socket.emitOpen();
			socket.emitMessage({
				type: 'session',
				busy: false,
				owner: false,
				transport: null,
				connected: true
			});

			consoleState.updateCommand('pwd');
			expect(consoleState.sendCommand()).toBe(true);
			socket.emitMessage({
				type: 'output',
				phase: 'final',
				text: '/tmp/project'
			});
			socket.emitMessage({
				type: 'session',
				busy: false,
				owner: false,
				transport: null,
				connected: true
			});
			expect(consoleState.currentPrompt).toBe('/tmp/project $');

			socket.emitMessage({
				type: 'session',
				busy: true,
				owner: false,
				transport: 'telegram',
				connected: true
			});

			expect(consoleState.currentDirectory).toBeNull();
			expect(consoleState.currentPrompt).toBe('$');
		});

		consoleState.destroy();
		cleanup?.();
	});

	it('reconnects after unexpected close', async () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			socket.emitOpen();
			socket.emitClose();

			expect(consoleState.isConnected).toBe(false);
			expect(mockNotifications.warning).toHaveBeenCalledWith(
				'USB terminal connection lost. Reconnecting...',
				5000
			);
		});

		await vi.advanceTimersByTimeAsync(3000);
		expect(MockWebSocket.instances).toHaveLength(2);

		consoleState.destroy();
		cleanup?.();
	});

	it('does not churn the socket on page visibility changes', () => {
		let cleanup: (() => void) | undefined;
		let consoleState!: ReturnType<typeof useUsbTerminalConsole>;

		cleanup = $effect.root(() => {
			consoleState = useUsbTerminalConsole();
			consoleState.init();

			const socket = MockWebSocket.instances[0];
			socket.emitOpen();

			Object.defineProperty(document, 'hidden', {
				value: true,
				configurable: true
			});
			document.dispatchEvent(new Event('visibilitychange'));

			expect(consoleState.isConnected).toBe(true);
			expect(MockWebSocket.instances).toHaveLength(1);

			Object.defineProperty(document, 'hidden', {
				value: false,
				configurable: true
			});
			document.dispatchEvent(new Event('visibilitychange'));

			expect(consoleState.isConnected).toBe(true);
			expect(MockWebSocket.instances).toHaveLength(1);
		});

		consoleState.destroy();
		cleanup?.();
	});
});
