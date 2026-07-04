/**
 * @file useSettings.svelte.ts
 * @brief Generic composable hook for managing settings state, API interactions, and validation.
 */
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { Logger } from '$lib/services/core/Logger';
import type { SettingsFeedback } from '$lib/utils/api/settingsFeedback';

function cloneValue<T>(value: T): T {
	if (typeof structuredClone === 'function') {
		return structuredClone(value);
	}
	return JSON.parse(JSON.stringify(value)) as T;
}

function snapshotValue<T>(value: T): T {
	return $state.snapshot(value) as T;
}

function defaultEquals<T>(left: T, right: T): boolean {
	return JSON.stringify(left) === JSON.stringify(right);
}

// Configuration options for the hook
export interface UseSettingsOptions<T, E = Record<string, boolean>> {
	// Function to fetch settings from API
	load: () => Promise<T>;
	// Function to save settings to API
	save: (settings: T) => Promise<T>;
	// Optional validation function. Returns populated errors object if failed, or null/false if passed.
	validate?: (settings: T, errors: E) => boolean;
	// Optional: guard that decides whether validation should run for current settings.
	// When it returns false, validation errors are cleared and save may continue.
	shouldValidate?: (settings: T) => boolean;
	// Optional callbacks
	onLoadSuccess?: (settings: T) => void;
	onSaveSuccess?: (settings: T) => void;
	equals?: (current: T, saved: T) => boolean;
	feedback?: SettingsFeedback<T>;
}

// Return type of the hook
export interface UseSettingsResult<T, E> {
	/** Silent refresh for polling — does NOT trigger loading spinner */
	refreshSettings: () => Promise<void>;
	settings: T;
	savedSettings: T | null;
	errors: E;
	errorMessage: string | null;
	loading: boolean;
	saving: boolean;
	hasChanges: boolean;
	setSettings: (settings: T) => void;
	updateSetting: <K extends keyof T>(key: K, value: T[K]) => void;
	loadSettings: () => Promise<void>;
	saveSettingsNow: () => Promise<boolean>;
	saveSettingsSilentlyNow: () => Promise<boolean>;
	saveSettings: () => void;
	resetSettings: () => void;
}

export function useSettings<T extends object, E extends object>(
	defaultSettings: T,
	defaultErrors: E,
	options: UseSettingsOptions<T, E>
): UseSettingsResult<T, E> {
	// State
	let settings = $state<T>(cloneValue(defaultSettings));
	let savedSettings = $state<T | null>(null);
	let errors = $state<E>(cloneValue(defaultErrors));
	let errorMessage = $state<string | null>(null);
	let loading = $state(true);
	let saving = $state(false);
	let latestLoadRequestId = 0;
	let latestMutationId = 0;

	// Derived
	let hasChanges = $derived.by(() => {
		if (!savedSettings) return false;
		const equals = options.equals ?? defaultEquals<T>;
		return !equals(settings, savedSettings);
	});

	function clearErrors() {
		for (const key in errors) {
			(errors as Record<string, unknown>)[key] = false;
		}
	}

	function areEqual(left: T, right: T): boolean {
		return (options.equals ?? defaultEquals<T>)(left, right);
	}

	function resolveFallbackMessage(context: 'load' | 'save'): string {
		return context === 'load'
			? m.settings_load_error({ locale: i18n.languageTag })
			: m.settings_save_error({ locale: i18n.languageTag });
	}

	function resolveErrorMessage(error: unknown, context: 'load' | 'save'): string {
		return (
			options.feedback?.resolveErrorMessage?.(error, context) ??
			toUserRequestErrorMessage(error, {
				fallbackMessage: resolveFallbackMessage(context)
			})
		);
	}

	function applyLoadedSettings(data: T) {
		settings = cloneValue(data);
		savedSettings = cloneValue(data);
		errorMessage = null;
		options.onLoadSuccess?.(data);
	}

	function isStaleLoad(requestId: number, mutationIdAtStart: number): boolean {
		return requestId !== latestLoadRequestId || mutationIdAtStart !== latestMutationId;
	}

	async function runLoad({ silent }: { silent: boolean }) {
		const requestId = ++latestLoadRequestId;
		const mutationIdAtStart = latestMutationId;

		if (!silent) {
			loading = true;
		}

		try {
			const data = await options.load();
			if (isStaleLoad(requestId, mutationIdAtStart)) {
				return;
			}
			applyLoadedSettings(data);
		} catch (error) {
			if (getRequestAbortKind(error) === 'abort') return;

			Logger.error(silent ? 'Settings silent refresh error:' : 'Settings load error:', error);

			if (silent || isStaleLoad(requestId, mutationIdAtStart)) {
				return;
			}

			errorMessage = resolveErrorMessage(error, 'load');
			options.feedback?.onError?.({ message: errorMessage, error, context: 'load' });
		} finally {
			if (!silent && requestId === latestLoadRequestId) {
				loading = false;
			}
		}
	}

	// Actions
	async function loadSettings() {
		await runLoad({ silent: false });
	}

	/**
	 * Silent refresh for polling — does NOT trigger loading spinner.
	 * Failures are logged but not shown to user.
	 */
	async function refreshSettings() {
		await runLoad({ silent: true });
	}

	function validate(): boolean {
		if (!options.validate) return true;

		if (options.shouldValidate?.(settings) === false) {
			clearErrors();
			return true;
		}

		// Run custom validation
		const hasError = options.validate(settings, errors);
		if (hasError) {
			options.feedback?.onValidationError?.();
			return false;
		}
		return true;
	}

	async function doSave({ notifySuccess }: { notifySuccess: boolean }): Promise<boolean> {
		if (saving) return false;
		const mutationId = ++latestMutationId;
		const requestSettings = cloneValue(snapshotValue(settings));
		saving = true;
		try {
			const updated = await options.save(requestSettings);
			if (mutationId !== latestMutationId) {
				return false;
			}
			if (areEqual(snapshotValue(settings), requestSettings)) {
				settings = cloneValue(updated);
			}
			savedSettings = cloneValue(updated);
			errorMessage = null;
			options.onSaveSuccess?.(updated);
			if (notifySuccess) {
				options.feedback?.onSaveSuccess?.(updated);
			}
			return true;
		} catch (error) {
			Logger.error('Settings save error:', error);
			if (getRequestAbortKind(error) === 'abort') return false;
			errorMessage = resolveErrorMessage(error, 'save');

			options.feedback?.onError?.({ message: errorMessage, error, context: 'save' });
			return false;
		} finally {
			saving = false;
		}
	}

	async function saveSettingsNow(): Promise<boolean> {
		if (!hasChanges) return false;
		if (!validate()) return false;
		return doSave({ notifySuccess: true });
	}

	async function saveSettingsSilentlyNow(): Promise<boolean> {
		if (!hasChanges) return false;
		if (!validate()) return false;
		return doSave({ notifySuccess: false });
	}

	function saveSettings() {
		void saveSettingsNow();
	}

	function setSettings(nextSettings: T) {
		settings = cloneValue(snapshotValue(nextSettings));
	}

	function updateSetting<K extends keyof T>(key: K, value: T[K]) {
		settings[key] = value;
		// Auto-clear error for this field if it exists
		if (key in errors) {
			(errors as Record<string, unknown>)[key as string] = false;
		}
	}

	function resetSettings() {
		if (savedSettings) {
			settings = cloneValue(savedSettings);
			clearErrors();
			errorMessage = null;
		}
	}

	return {
		get settings() {
			return settings;
		},
		get savedSettings() {
			return savedSettings;
		},
		get errors() {
			return errors;
		},
		get errorMessage() {
			return errorMessage;
		},
		get loading() {
			return loading;
		},
		set loading(v) {
			loading = v;
		},
		get saving() {
			return saving;
		},
		get hasChanges() {
			return hasChanges;
		},
		setSettings,
		loadSettings,
		refreshSettings,
		saveSettingsNow,
		saveSettingsSilentlyNow,
		saveSettings,
		updateSetting,
		resetSettings
	};
}
