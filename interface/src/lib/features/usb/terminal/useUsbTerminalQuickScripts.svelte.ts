import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import {
	MacroApiService,
	type ScriptFile,
	type ScriptStatus
} from '$lib/services/api/integrations/MacroApiService';
import {
	KeyboardApiService,
	type KeyboardPressPayload,
	type KeyboardTypePayload
} from '$lib/services/api/integrations/KeyboardApiService';
import { useMacroStatusSync } from '$lib/features/system/status/useMacroStatusSync.svelte';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

type MacroApi = Pick<
	MacroApiService,
	'getSettings' | 'getStatus' | 'listScripts' | 'runScript' | 'stopScript'
>;
type KeyboardApi = Pick<KeyboardApiService, 'pressKey' | 'typeText'>;

interface MacroChannelStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
}

export interface HostQuickAction {
	id: string;
	name: string;
}

interface UsbTerminalQuickScriptsDeps {
	createApi?: () => MacroApi;
	createKeyboardApi?: () => KeyboardApi;
	channelStore?: MacroChannelStore;
	shouldInit?: () => boolean;
	delay?: (ms: number) => Promise<void>;
}

type PendingAction = 'run' | 'stop' | 'host' | null;
type HostActionStep =
	| { type: 'press'; payload: KeyboardPressPayload; delayAfterMs: number }
	| { type: 'type'; payload: KeyboardTypePayload; delayAfterMs: number };

const HOST_QUICK_ACTIONS: HostQuickAction[] = [
	{
		id: 'macos-focus-terminal',
		name: 'Focus macOS Terminal'
	}
];

const HOST_ACTION_STEPS: Record<string, HostActionStep[]> = {
	'macos-focus-terminal': [
		{ type: 'press', payload: { key: 177 }, delayAfterMs: 400 },
		{ type: 'press', payload: { keys: [131, 32] }, delayAfterMs: 1400 },
		{ type: 'type', payload: { text: 'Terminal', enter: true }, delayAfterMs: 4000 },
		{ type: 'press', payload: { keys: [128, 99] }, delayAfterMs: 300 }
	]
};

function sortScripts(scripts: ScriptFile[]): ScriptFile[] {
	return [...scripts].sort((left, right) => left.name.localeCompare(right.name));
}

export function useUsbTerminalQuickScripts(deps: UsbTerminalQuickScriptsDeps = {}) {
	const apiClient = deps.createApi && deps.createKeyboardApi ? null : useApiClient();

	let scripts = $state<ScriptFile[]>([]);
	let macrosEnabled = $state<boolean | null>(null);
	let loading = $state(true);
	let error = $state<string | null>(null);
	let pendingScriptName = $state<string | null>(null);
	let pendingHostActionId = $state<string | null>(null);
	let pendingAction = $state<PendingAction>(null);

	let scriptsAbort: AbortController | null = null;
	let settingsAbort: AbortController | null = null;
	let disposed = false;

	function localeOptions() {
		return { locale: i18n.languageTag };
	}

	function createApi(): MacroApi {
		if (deps.createApi) {
			return deps.createApi();
		}

		return apiClient!.createService(MacroApiService);
	}

	function createKeyboardApi(): KeyboardApi {
		if (deps.createKeyboardApi) {
			return deps.createKeyboardApi();
		}

		return apiClient!.createService(KeyboardApiService);
	}

	function wait(ms: number) {
		if (deps.delay) {
			return deps.delay(ms);
		}

		return new Promise<void>((resolve) => window.setTimeout(resolve, ms));
	}

	function clearAborts() {
		scriptsAbort?.abort();
		settingsAbort?.abort();
		scriptsAbort = null;
		settingsAbort = null;
	}

	function getStatus(): ScriptStatus | null {
		return macroStatus.status;
	}

	function setErrorMessage(err: unknown, fallbackMessage: string) {
		if (getRequestAbortKind(err) === 'abort') return;
		error = toUserRequestErrorMessage(err, { fallbackMessage });
	}

	const macroStatus = useMacroStatusSync({
		createApi,
		channelStore: deps.channelStore,
		onError: (err) => {
			setErrorMessage(err, m.usb_terminal_quick_scripts_load_error(localeOptions()));
		}
	});

	async function loadScripts() {
		scriptsAbort?.abort();
		const controller = new AbortController();
		scriptsAbort = controller;

		try {
			const nextScripts = await createApi().listScripts(controller.signal);
			if (disposed || controller.signal.aborted) return;
			scripts = sortScripts(nextScripts);
		} catch (err) {
			if (controller.signal.aborted) return;
			setErrorMessage(err, m.usb_terminal_quick_scripts_load_error(localeOptions()));
		} finally {
			if (scriptsAbort === controller) {
				scriptsAbort = null;
			}
		}
	}

	async function loadSettings() {
		settingsAbort?.abort();
		const controller = new AbortController();
		settingsAbort = controller;

		try {
			const settings = await createApi().getSettings(controller.signal);
			if (disposed || controller.signal.aborted) return;
			macrosEnabled = settings.enabled;
		} catch (err) {
			if (controller.signal.aborted) return;
			setErrorMessage(err, m.usb_terminal_quick_scripts_load_error(localeOptions()));
		} finally {
			if (settingsAbort === controller) {
				settingsAbort = null;
			}
		}
	}

	async function init() {
		disposed = false;
		loading = true;
		error = null;

		try {
			await Promise.all([loadScripts(), loadSettings(), macroStatus.init()]);
		} finally {
			if (!disposed) {
				loading = false;
			}
		}
	}

	function destroy() {
		disposed = true;
		clearAborts();
		macroStatus.destroy();
	}

	$effect(() => {
		if (!(deps.shouldInit?.() ?? false)) return;

		void init();

		return () => {
			destroy();
		};
	});

	async function runScript(name: string) {
		if (!name || pendingAction || macrosEnabled === false) return false;

		error = null;
		pendingScriptName = name;
		pendingAction = 'run';

		try {
			const result = await createApi().runScript(name);
			if (!result.ok) {
				error = result.error ?? m.usb_terminal_quick_scripts_run_error(localeOptions());
				return false;
			}

			await macroStatus.refreshStatus();
			return true;
		} catch (err) {
			setErrorMessage(err, m.usb_terminal_quick_scripts_run_error(localeOptions()));
			return false;
		} finally {
			pendingScriptName = null;
			pendingAction = null;
		}
	}

	async function stopScript() {
		if (pendingAction || getStatus()?.status !== 'RUNNING') return false;

		error = null;
		pendingScriptName = getStatus()?.current_script || null;
		pendingAction = 'stop';

		try {
			const result = await createApi().stopScript();
			if (!result.ok) {
				error = result.error ?? m.usb_terminal_quick_scripts_stop_error(localeOptions());
				return false;
			}

			await macroStatus.refreshStatus();
			return true;
		} catch (err) {
			setErrorMessage(err, m.usb_terminal_quick_scripts_stop_error(localeOptions()));
			return false;
		} finally {
			pendingScriptName = null;
			pendingAction = null;
		}
	}

	async function runHostAction(id: string) {
		const steps = HOST_ACTION_STEPS[id];
		if (!steps || pendingAction || getStatus()?.status === 'RUNNING') return false;

		error = null;
		pendingHostActionId = id;
		pendingAction = 'host';

		try {
			const keyboardApi = createKeyboardApi();
			for (const step of steps) {
				if (step.type === 'press') {
					await keyboardApi.pressKey(step.payload);
				} else {
					await keyboardApi.typeText(step.payload);
				}
				if (step.delayAfterMs > 0) {
					await wait(step.delayAfterMs);
				}
			}
			return true;
		} catch (err) {
			setErrorMessage(err, m.usb_terminal_quick_scripts_run_error(localeOptions()));
			return false;
		} finally {
			pendingHostActionId = null;
			pendingAction = null;
		}
	}

	function isRunningScript(name: string) {
		const status = getStatus();
		return status?.status === 'RUNNING' && status.current_script === name;
	}

	function isScriptDisabled(name: string, terminalBusy = false) {
		if (loading || terminalBusy || macrosEnabled === false || pendingAction === 'stop') return true;
		const status = getStatus();
		if (status?.status !== 'RUNNING') return pendingAction === 'run';
		return status.current_script !== name;
	}

	function isHostActionDisabled(terminalBusy = false) {
		return loading || terminalBusy || pendingAction !== null || getStatus()?.status === 'RUNNING';
	}

	return {
		get hostActions() {
			return HOST_QUICK_ACTIONS;
		},
		get scripts() {
			return scripts;
		},
		get macrosEnabled() {
			return macrosEnabled;
		},
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		get status() {
			return getStatus();
		},
		get pendingScriptName() {
			return pendingScriptName;
		},
		get pendingHostActionId() {
			return pendingHostActionId;
		},
		get pendingAction() {
			return pendingAction;
		},
		get shouldShowSection() {
			return (
				loading || HOST_QUICK_ACTIONS.length > 0 || scripts.length > 0 || macrosEnabled === false
			);
		},
		get isTerminalCommandDisabled() {
			return pendingAction !== null || getStatus()?.status === 'RUNNING';
		},
		get runningScriptName() {
			return getStatus()?.status === 'RUNNING' ? (getStatus()?.current_script ?? '') : '';
		},
		runHostAction,
		runScript,
		stopScript,
		isRunningScript,
		isScriptDisabled,
		isHostActionDisabled,
		init,
		destroy
	};
}
