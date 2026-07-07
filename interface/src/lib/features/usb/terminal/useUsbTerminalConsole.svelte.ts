import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { buildWebSocketUrl } from '$lib/utils/ws/buildWebSocketUrl';
import { ManagedSocketTransport } from '$lib/utils/ws/managedSocketTransport';
import { localizeTerminalMessage } from './terminalMessageCatalog';
import {
	appendEntry as appendTranscriptEntry,
	appendOutputEntry as appendTranscriptOutputEntry,
	formatCommandEntry,
	formatPrompt,
	getDirectoryProbeKind,
	getLastNonEmptyLine,
	type DirectoryProbeKind,
	type EntryPhase,
	type OutputPhase,
	type SessionMessage,
	type TerminalEntry,
	type TerminalMessage,
	type TerminalTransport
} from './terminalTranscriptModel';

const RETRY_DELAY_MS = 3000;
const CONNECT_TIMEOUT_MS = 10000;
const TOAST_DURATION_MS = 5000;
const TOAST_DEDUP_WINDOW_MS = 1500;
const DIAGNOSTIC_COMMAND = 'id';

interface UsbTerminalConsoleDeps {
	shouldInit?: () => boolean;
}

export function useUsbTerminalConsole(deps: UsbTerminalConsoleDeps = {}) {
	const session = useSessionAccess();
	let nextEntryId = 1;

	let isConnected = $state(false);
	let busy = $state(false);
	let owner = $state(false);
	let transport = $state<TerminalTransport>(null);
	let command = $state('');
	let entries = $state.raw<TerminalEntry[]>([]);
	let currentDirectory = $state<string | null>(null);
	let pendingDirectoryProbe = $state<DirectoryProbeKind>(null);
	let activeOutputEntryId = $state<number | null>(null);
	let commandInFlight = $state(false);
	let pendingCommandOutput = $state<{
		observedOwnedSession: boolean;
		sawOutput: boolean;
	} | null>(null);
	let lastToastKey: string | null = null;
	let lastToastTimer: ReturnType<typeof setTimeout> | null = null;

	function showToast(type: 'info' | 'success' | 'warning' | 'error', message: string | null) {
		if (!message) return;

		const toastKey = `${type}:${message}`;
		if (lastToastKey === toastKey) return;

		lastToastKey = toastKey;
		if (lastToastTimer) {
			clearTimeout(lastToastTimer);
		}
		lastToastTimer = setTimeout(() => {
			if (lastToastKey === toastKey) {
				lastToastKey = null;
			}
			lastToastTimer = null;
		}, TOAST_DEDUP_WINDOW_MS);

		notifications[type](message, TOAST_DURATION_MS);
	}

	function clearToastDedupe() {
		if (lastToastTimer) {
			clearTimeout(lastToastTimer);
		}
		lastToastTimer = null;
		lastToastKey = null;
	}

	function canConnect() {
		if (typeof window === 'undefined') return false;
		if (!session.isAdmin) {
			return false;
		}
		return true;
	}

	function resetPromptState() {
		currentDirectory = null;
		pendingDirectoryProbe = null;
		activeOutputEntryId = null;
		commandInFlight = false;
		pendingCommandOutput = null;
	}

	function appendEntry(phase: EntryPhase, text: string) {
		const appended = appendTranscriptEntry(entries, nextEntryId, phase, text);
		entries = appended.entries;
		nextEntryId = appended.nextEntryId;
	}

	function appendOutputEntry(phase: OutputPhase, text: string): string | null {
		const appended = appendTranscriptOutputEntry(
			entries,
			nextEntryId,
			activeOutputEntryId,
			phase,
			text
		);
		entries = appended.entries;
		nextEntryId = appended.nextEntryId;
		activeOutputEntryId = appended.activeOutputEntryId;
		return appended.mergedText;
	}

	function maybeUpdateCurrentDirectory(outputText: string | null) {
		if (!pendingDirectoryProbe || !outputText) return;
		const nextDirectory = getLastNonEmptyLine(outputText);
		if (nextDirectory) {
			currentDirectory = nextDirectory;
		}
	}

	function noOutputMessage() {
		return m.usb_terminal_no_output_hint({ locale: i18n.languageTag });
	}

	function markPendingCommandOutputSeen() {
		if (!pendingCommandOutput) return;
		pendingCommandOutput = {
			...pendingCommandOutput,
			sawOutput: true
		};
	}

	function maybeAppendNoOutputNotice() {
		if (!pendingCommandOutput?.observedOwnedSession || pendingCommandOutput.sawOutput) {
			return;
		}

		const message = noOutputMessage();
		appendEntry('status', message);
		showToast('warning', message);
		pendingCommandOutput = null;
	}

	function setSession(message: SessionMessage) {
		const nextBusy = Boolean(message.busy);
		const nextOwner = Boolean(message.owner);
		const nextTransport =
			message.transport === 'telegram' || message.transport === 'ws' ? message.transport : null;

		if (pendingCommandOutput && nextBusy && nextOwner && nextTransport === 'ws') {
			pendingCommandOutput = {
				...pendingCommandOutput,
				observedOwnedSession: true
			};
			commandInFlight = false;
		}

		busy = nextBusy;
		owner = nextOwner;
		transport = nextTransport;

		if (!nextBusy) {
			commandInFlight = false;
			activeOutputEntryId = null;
			pendingDirectoryProbe = null;
			maybeAppendNoOutputNotice();
			if (pendingCommandOutput?.observedOwnedSession) {
				pendingCommandOutput = null;
			}
		}
		if (nextBusy && (!nextOwner || nextTransport !== 'ws')) {
			resetPromptState();
		}
	}

	function handleMessage(raw: string) {
		let parsed: TerminalMessage;
		try {
			parsed = JSON.parse(raw) as TerminalMessage;
		} catch {
			showToast('error', m.usb_terminal_parse_error({ locale: i18n.languageTag }));
			return;
		}

		if (parsed.type === 'session') {
			setSession(parsed);
			return;
		}

		if (parsed.type === 'ack') {
			if (parsed.ok) {
				showToast('info', localizeTerminalMessage(parsed.message, i18n.languageTag));
			} else {
				if (parsed.action === 'execute') {
					pendingDirectoryProbe = null;
					activeOutputEntryId = null;
					commandInFlight = false;
					pendingCommandOutput = null;
				}
				showToast(
					'error',
					localizeTerminalMessage(parsed.message, i18n.languageTag) ??
						m.usb_terminal_request_failed({ locale: i18n.languageTag })
				);
			}
			return;
		}

		if (parsed.type === 'output') {
			if ((parsed.text ?? '').length > 0) {
				markPendingCommandOutputSeen();
			}
			const mergedOutput = appendOutputEntry(parsed.phase, parsed.text ?? '');
			if (parsed.phase === 'final') {
				maybeUpdateCurrentDirectory(mergedOutput);
				pendingDirectoryProbe = null;
				if (pendingCommandOutput?.sawOutput) {
					pendingCommandOutput = null;
				}
			}
			if (parsed.phase === 'interrupted') {
				pendingDirectoryProbe = null;
				pendingCommandOutput = null;
			}
			return;
		}

		showToast(
			'error',
			localizeTerminalMessage(parsed.message, i18n.languageTag) ??
				m.usb_terminal_error_generic({ locale: i18n.languageTag })
		);
	}

	const socketTransport = new ManagedSocketTransport({
		getUrl: () => buildWebSocketUrl('/ws/usbterminal'),
		canConnect,
		reconnectDelayMs: RETRY_DELAY_MS,
		connectTimeoutMs: CONNECT_TIMEOUT_MS,
		onBeforeConnect: () => {
			resetPromptState();
		},
		onOpen: () => {
			isConnected = true;
			showToast('info', m.usb_terminal_connection_established({ locale: i18n.languageTag }));
		},
		onMessage: (event) => {
			if (typeof event.data === 'string') {
				handleMessage(event.data);
			}
		},
		// The reconnect close handler below is the user-facing signal.
		// We avoid showing a second toast here for the same network failure.
		onError: () => {},
		onClose: ({ intentional }) => {
			if (intentional) {
				return;
			}

			isConnected = false;
			owner = false;
			showToast(
				'warning',
				m.usb_terminal_connection_lost_reconnecting({
					locale: i18n.languageTag
				})
			);
		},
		shouldReconnect: ({ intentional }) => !intentional
	});

	function connect() {
		socketTransport.connect();
	}

	function resetConnectionState() {
		isConnected = false;
		owner = false;
		resetPromptState();
	}

	function destroyTransport() {
		resetConnectionState();
		clearToastDedupe();
		socketTransport.destroy();
	}

	function sendFrame(payload: Record<string, unknown>) {
		const ws = socketTransport.ws;
		if (!ws || ws.readyState !== WebSocket.OPEN) {
			showToast('error', m.usb_terminal_not_connected({ locale: i18n.languageTag }));
			return false;
		}

		ws.send(JSON.stringify(payload));
		return true;
	}

	function updateCommand(value: string) {
		command = value;
	}

	function sendCommand(commandOverride?: string) {
		const trimmed = (commandOverride ?? command).trim();
		if (!trimmed.length || busy || commandInFlight || !isConnected) return false;
		if (!sendFrame({ type: 'execute', command: trimmed })) return false;

		commandInFlight = true;
		pendingDirectoryProbe = getDirectoryProbeKind(trimmed);
		activeOutputEntryId = null;
		pendingCommandOutput = {
			observedOwnedSession: false,
			sawOutput: false
		};
		appendEntry('command', formatCommandEntry(trimmed, currentDirectory));
		if (commandOverride === undefined) {
			command = '';
		}
		return true;
	}

	function sendDiagnosticCommand() {
		return sendCommand(DIAGNOSTIC_COMMAND);
	}

	function sendCancel() {
		if (!busy || !owner || !isConnected) return false;
		return sendFrame({ type: 'cancel' });
	}

	function clearEntries() {
		entries = [];
		activeOutputEntryId = null;
		pendingCommandOutput = null;
	}

	function init() {
		connect();
	}

	function destroy() {
		destroyTransport();
	}

	$effect(() => {
		if (!(deps.shouldInit?.() ?? false)) return;

		init();

		return () => {
			destroy();
		};
	});

	return {
		get isConnected() {
			return isConnected;
		},
		get busy() {
			return busy;
		},
		get owner() {
			return owner;
		},
		get transport() {
			return transport;
		},
		get command() {
			return command;
		},
		get entries() {
			return entries;
		},
		get currentDirectory() {
			return currentDirectory;
		},
		get currentPrompt() {
			return formatPrompt(currentDirectory);
		},
		get canExecute() {
			return isConnected && !busy && !commandInFlight;
		},
		get canCancel() {
			return isConnected && busy && owner;
		},
		updateCommand,
		sendCommand,
		sendDiagnosticCommand,
		sendCancel,
		clearEntries,
		init,
		destroy
	};
}
