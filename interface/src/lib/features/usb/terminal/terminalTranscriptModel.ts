const SHELL_OPERATOR_PATTERN = /&&|\|\||[;|<>\n\r]/;

export type TerminalTransport = 'telegram' | 'ws' | null;
export type OutputPhase = 'partial' | 'final' | 'interrupted' | 'status';
export type EntryPhase = OutputPhase | 'command';
export type DirectoryProbeKind = 'cd' | 'pwd' | null;

export interface SessionMessage {
	type: 'session';
	busy: boolean;
	owner: boolean;
	transport: TerminalTransport;
	connected: boolean;
}

export interface AckMessage {
	type: 'ack';
	action: 'execute' | 'cancel' | 'status';
	ok: boolean;
	message: string;
}

export interface OutputMessage {
	type: 'output';
	phase: OutputPhase;
	text: string;
}

export interface ErrorMessage {
	type: 'error';
	message: string;
}

export type TerminalMessage = SessionMessage | AckMessage | OutputMessage | ErrorMessage;

export interface TerminalEntry {
	id: number;
	phase: EntryPhase;
	text: string;
}

export interface AppendEntryResult {
	entries: TerminalEntry[];
	nextEntryId: number;
}

export interface AppendOutputEntryResult extends AppendEntryResult {
	activeOutputEntryId: number | null;
	mergedText: string | null;
}

function isStandalonePwdCommand(command: string): boolean {
	return command.trim() === 'pwd';
}

function isStandaloneCdCommand(command: string): boolean {
	const trimmed = command.trim();
	if (!trimmed.startsWith('cd')) return false;
	if (trimmed.length > 2 && !/\s/.test(trimmed[2] ?? '')) return false;
	return !SHELL_OPERATOR_PATTERN.test(trimmed);
}

export function getDirectoryProbeKind(command: string): DirectoryProbeKind {
	if (isStandalonePwdCommand(command)) return 'pwd';
	if (isStandaloneCdCommand(command)) return 'cd';
	return null;
}

export function getLastNonEmptyLine(text: string): string | null {
	const lines = text
		.split(/\r?\n/)
		.map((line) => line.trim())
		.filter((line) => line.length > 0);
	return lines.length ? lines[lines.length - 1] : null;
}

export function formatPrompt(currentDirectory: string | null): string {
	return currentDirectory ? `${currentDirectory} $` : '$';
}

export function formatCommandEntry(command: string, currentDirectory: string | null): string {
	return `${formatPrompt(currentDirectory)} ${command}`;
}

export function appendEntry(
	entries: TerminalEntry[],
	nextEntryId: number,
	phase: EntryPhase,
	text: string
): AppendEntryResult {
	if (!text.length) {
		return { entries, nextEntryId };
	}

	return {
		entries: [
			...entries,
			{
				id: nextEntryId,
				phase,
				text
			}
		],
		nextEntryId: nextEntryId + 1
	};
}

export function appendOutputEntry(
	entries: TerminalEntry[],
	nextEntryId: number,
	activeOutputEntryId: number | null,
	phase: OutputPhase,
	text: string
): AppendOutputEntryResult {
	if (!text.length) {
		return {
			entries,
			nextEntryId,
			activeOutputEntryId,
			mergedText: null
		};
	}

	if (phase === 'status' || phase === 'interrupted') {
		const appended = appendEntry(entries, nextEntryId, phase, text);
		return {
			...appended,
			activeOutputEntryId: null,
			mergedText: text
		};
	}

	const lastEntry = entries[entries.length - 1];
	if (
		activeOutputEntryId !== null &&
		lastEntry &&
		lastEntry.id === activeOutputEntryId &&
		(lastEntry.phase === 'partial' || lastEntry.phase === 'final')
	) {
		const mergedText = `${lastEntry.text}${text}`;
		return {
			entries: [
				...entries.slice(0, -1),
				{
					...lastEntry,
					phase: phase === 'final' ? 'final' : 'partial',
					text: mergedText
				}
			],
			nextEntryId,
			activeOutputEntryId: phase === 'final' ? null : activeOutputEntryId,
			mergedText
		};
	}

	const entryId = nextEntryId;
	return {
		entries: [
			...entries,
			{
				id: entryId,
				phase,
				text
			}
		],
		nextEntryId: nextEntryId + 1,
		activeOutputEntryId: phase === 'partial' ? entryId : null,
		mergedText: text
	};
}
