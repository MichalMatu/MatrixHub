import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { toUserRequestErrorMessage } from '$lib/utils';
import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';
import {
	MacroApiService,
	type MacroActionResponse,
	type MacroSettings,
	type ScriptFile
} from '$lib/services/api/integrations/MacroApiService';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

type MacroApi = Pick<
	MacroApiService,
	| 'deleteScript'
	| 'getStatus'
	| 'getScriptContent'
	| 'getSettings'
	| 'listScripts'
	| 'runScript'
	| 'saveSettings'
	| 'stopScript'
	| 'uploadScript'
>;

interface MacroNotifications {
	error(message: string, duration?: number): void;
	success(message: string, duration?: number): void;
}

interface MacroManagementDeps {
	createApi?: () => MacroApi;
	notifications?: MacroNotifications;
}

const MAX_MACRO_FILENAME_LENGTH = 32;
const MAX_MACRO_SCRIPT_BYTES = 8192;
const MACRO_FILENAME_PATTERN = /^[A-Za-z0-9._-]+$/;

function byteLength(value: string) {
	if (typeof TextEncoder !== 'undefined') {
		return new TextEncoder().encode(value).length;
	}
	return value.length;
}

function isValidScriptFilename(filename: string) {
	return (
		filename.length > 0 &&
		filename.length <= MAX_MACRO_FILENAME_LENGTH &&
		!filename.includes('/') &&
		!filename.includes('\\') &&
		!filename.includes('..') &&
		!filename.endsWith('.tmp') &&
		MACRO_FILENAME_PATTERN.test(filename)
	);
}

export function useMacroManagement(deps: MacroManagementDeps = {}) {
	const apiClient = deps.createApi ? null : useApiClient();
	const toast = deps.notifications ?? notifications;

	let settings = $state<MacroSettings>({
		enabled: true,
		boot_script: '',
		boot_delay: 5000
	});
	let localSettings = $state<MacroSettings>({
		enabled: true,
		boot_script: '',
		boot_delay: 5000
	});
	let scriptList = $state<ScriptFile[]>([]);
	let loading = $state(true);
	let error = $state<string | null>(null);
	let settingsSaving = $state(false);
	let showEditor = $state(false);
	let isSavingScript = $state(false);
	let editFilename = $state('');
	let editContent = $state('');
	let isNew = $state(false);
	let settingsSynced = $state(false);

	let scriptsAbort: AbortController | null = null;
	let settingsAbort: AbortController | null = null;
	let editAbort: AbortController | null = null;
	let editRequestId = 0;
	let disposed = false;

	const hasSettingsChanges = $derived.by(() => {
		return (
			localSettings.enabled !== settings.enabled ||
			localSettings.boot_script !== settings.boot_script ||
			localSettings.boot_delay !== settings.boot_delay
		);
	});

	const scriptOptions = $derived.by(() => {
		const options = [
			{ label: m.macros_boot_script_disabled(), value: '' },
			...scriptList.map((script) => ({ label: script.name, value: script.name }))
		];

		const current = localSettings.boot_script || settings.boot_script;
		if (current && !options.some((option) => option.value === current)) {
			options.splice(1, 0, { label: current, value: current });
		}

		return options;
	});

	$effect(() => {
		if (loading) return;
		if (settingsSynced && hasSettingsChanges) return;

		localSettings.enabled = settings.enabled;
		localSettings.boot_script = settings.boot_script;
		localSettings.boot_delay = settings.boot_delay;
		settingsSynced = true;
	});

	function createApi(): MacroApi {
		if (deps.createApi) {
			return deps.createApi();
		}

		return apiClient!.createService(MacroApiService);
	}

	function dispose() {
		disposed = true;
		scriptsAbort?.abort();
		settingsAbort?.abort();
		editAbort?.abort();
		scriptsAbort = null;
		settingsAbort = null;
		editAbort = null;
	}

	async function init() {
		loading = true;
		error = null;
		disposed = false;

		await Promise.all([fetchSettings(), fetchScripts()]);
		if (disposed) return;
		loading = false;
	}

	async function fetchSettings() {
		settingsAbort?.abort();
		const controller = new AbortController();
		settingsAbort = controller;

		try {
			const nextSettings = await createApi().getSettings(controller.signal);
			if (disposed || controller.signal.aborted) return;
			settings = nextSettings;
		} catch (err) {
			if (controller.signal.aborted) return;
			console.error('Failed to load macro settings', err);
			error = m.settings_load_error();
		} finally {
			if (settingsAbort === controller) {
				settingsAbort = null;
			}
		}
	}

	async function fetchScripts() {
		scriptsAbort?.abort();
		const controller = new AbortController();
		scriptsAbort = controller;

		try {
			const scripts = await createApi().listScripts(controller.signal);
			if (disposed || controller.signal.aborted) return;
			scriptList = scripts;
		} catch (err) {
			if (controller.signal.aborted) return;
			console.error('Failed to load scripts', err);
			error = m.settings_load_error();
		} finally {
			if (scriptsAbort === controller) {
				scriptsAbort = null;
			}
		}
	}

	function handleNew() {
		editAbort?.abort();
		editAbort = null;
		editRequestId += 1;
		editFilename = '';
		editContent = '';
		isNew = true;
		showEditor = true;
	}

	async function handleEdit(name: string) {
		editAbort?.abort();
		const controller = new AbortController();
		editAbort = controller;
		const requestId = ++editRequestId;

		try {
			const content = await createApi().getScriptContent(name, controller.signal);
			if (disposed || controller.signal.aborted || requestId !== editRequestId) return;
			editFilename = name;
			editContent = content;
			isNew = false;
			showEditor = true;
		} catch (err) {
			if (controller.signal.aborted) return;
			console.error('Failed to load script content', err);
			toast.error(m.macros_error_script_content(), 3000);
		} finally {
			if (editAbort === controller) {
				editAbort = null;
			}
		}
	}

	async function saveScript(data: { filename: string; content: string }) {
		const filename = data.filename.trim();
		if (isSavingScript) return;
		if (!filename) {
			toast.error(m.macros_error_filename_required(), 3000);
			return;
		}
		if (!isValidScriptFilename(filename)) {
			toast.error(m.macros_error_filename_invalid(), 3000);
			return;
		}
		if (byteLength(data.content) > MAX_MACRO_SCRIPT_BYTES) {
			toast.error(m.macros_error_script_too_large(), 3000);
			return;
		}

		isSavingScript = true;

		try {
			const result = await createApi().uploadScript(filename, data.content);
			if (!isSuccessful(result, 'saved')) {
				toast.error(result.error ?? m.macros_error_save_script(), 3000);
				return;
			}

			showEditor = false;
			await fetchScripts();
			toast.success(m.toast_macro_script_saved(), 3000);
		} catch (err) {
			console.error(err);
			toast.error(m.macros_error_save_script(), 3000);
		} finally {
			isSavingScript = false;
		}
	}

	async function saveSettings() {
		return persistSettings();
	}

	async function persistSettings(options: { throwOnError?: boolean } = {}): Promise<boolean> {
		const { throwOnError = false } = options;
		if (settingsSaving || !hasSettingsChanges) {
			if (throwOnError) {
				throw new Error('Macro settings save skipped');
			}
			return false;
		}
		settingsSaving = true;

		try {
			const savedSettings = await createApi().saveSettings(localSettings);
			settings = savedSettings;
			localSettings.enabled = savedSettings.enabled;
			localSettings.boot_script = savedSettings.boot_script;
			localSettings.boot_delay = savedSettings.boot_delay;
			settingsSynced = true;
			toast.success(m.macro_msg_saved(), 3000);
			return true;
		} catch (err) {
			console.error(err);
			toast.error(
				toUserRequestErrorMessage(err, {
					fallbackMessage: m.settings_save_error()
				}),
				3000
			);
			if (throwOnError) {
				throw err;
			}
			return false;
		} finally {
			settingsSaving = false;
		}
	}

	function requiresSettingsRestart() {
		return localSettings.enabled !== settings.enabled;
	}

	function resetSettings() {
		localSettings.enabled = settings.enabled;
		localSettings.boot_script = settings.boot_script;
		localSettings.boot_delay = settings.boot_delay;
		settingsSynced = true;
	}

	function confirmSaveSettings() {
		if (!hasSettingsChanges) return;

		if (!requiresSettingsRestart()) {
			void saveSettings();
			return;
		}

		confirmRestartAndSave(() => persistSettings({ throwOnError: true }), {
			message: m.restart_confirm_msg_generic({ locale: i18n.languageTag })
		});
	}

	async function runScript(name: string) {
		try {
			const result = await createApi().runScript(name);
			if (!isSuccessful(result, 'started')) {
				toast.error(result.error ?? m.macros_error_start(), 3000);
				return;
			}
			toast.success(m.macros_msg_started(), 3000);
		} catch (err) {
			console.error(err);
			toast.error(m.macros_error_start(), 3000);
		}
	}

	async function stopScript() {
		try {
			const result = await createApi().stopScript();
			if (!isSuccessful(result, 'stopped')) {
				toast.error(result.error ?? m.macros_error_stop(), 3000);
				return;
			}
			toast.success(m.macros_msg_stopped(), 3000);
		} catch (err) {
			console.error(err);
			toast.error(m.macros_error_stop(), 3000);
		}
	}

	async function deleteScript(name: string) {
		try {
			const result = await createApi().deleteScript(name);
			if (!isSuccessful(result, 'deleted')) {
				toast.error(result.error ?? m.macros_error_delete(), 3000);
				return;
			}
			await fetchScripts();
			toast.success(m.macros_msg_deleted(), 3000);
		} catch (err) {
			console.error(err);
			toast.error(m.macros_error_delete(), 3000);
		}
	}

	function isSuccessful(
		result: MacroActionResponse,
		expectedStatus: NonNullable<MacroActionResponse['status']>
	) {
		return result.ok && result.status === expectedStatus;
	}

	return {
		get apiService() {
			return createApi();
		},
		get settings() {
			return settings;
		},
		localSettings,
		get scriptList() {
			return scriptList;
		},
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		get settingsSaving() {
			return settingsSaving;
		},
		get showEditor() {
			return showEditor;
		},
		set showEditor(value: boolean) {
			showEditor = value;
		},
		get isSavingScript() {
			return isSavingScript;
		},
		get editFilename() {
			return editFilename;
		},
		set editFilename(value: string) {
			editFilename = value;
		},
		get editContent() {
			return editContent;
		},
		set editContent(value: string) {
			editContent = value;
		},
		get isNew() {
			return isNew;
		},
		get hasSettingsChanges() {
			return hasSettingsChanges;
		},
		get requiresSettingsRestart() {
			return requiresSettingsRestart();
		},
		get scriptOptions() {
			return scriptOptions;
		},
		init,
		dispose,
		fetchSettings,
		fetchScripts,
		handleNew,
		handleEdit,
		saveScript,
		saveSettings,
		resetSettings,
		confirmSaveSettings,
		runScript,
		stopScript,
		deleteScript
	};
}
