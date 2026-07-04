import * as m from '$lib/paraglide/messages';
import { KeyboardApiService } from '$lib/services/api/integrations/KeyboardApiService';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import {
	KEY_LEFT_CTRL,
	KEY_LEFT_SHIFT,
	KEY_LEFT_ALT,
	KEY_LEFT_GUI,
	KEY_RIGHT_ALT,
	SPECIAL_KEYS
} from './config/constants';
import {
	getPhysicalKeyboardButton,
	getPhysicalKeyboardModifier,
	isCapsLockCode
} from './config/physicalKeyMap';
import { LAYOUT_CONFIG } from './config/layouts';
import type { Language } from './config/types';
import { createSimpleKeyboardManager } from './components/SimpleKeyboardManager';

type StatusKind = 'success' | 'error' | 'info' | '';

interface KeyboardInputAdapter {
	getInput(): string;
}

interface KeyboardManagerAdapter {
	keyboard: KeyboardInputAdapter | null;
	setMode(mode: Language): void;
	setModifiers(modifiers: number[]): void;
	setCapsLock(isLocked: boolean): void;
	setPressedButtons(buttons: string[]): void;
	setInput(input: string): void;
	clearInput(): void;
	destroy(): void;
}

type KeyboardApi = Pick<KeyboardApiService, 'typeText' | 'pressKey'>;

interface KeyboardManagementDeps {
	createApi?: () => KeyboardApi;
	createManager?: (
		onChange: (input: string) => void,
		onKeyPress: (button: string) => void
	) => KeyboardManagerAdapter | Promise<KeyboardManagerAdapter>;
}

export function useKeyboardManagement(
	getToken: () => string = () => '',
	deps: KeyboardManagementDeps = {}
) {
	const apiClient = deps.createApi ? null : useApiClient();

	let manager = $state<KeyboardManagerAdapter | undefined>();
	let textQueue = $state('');
	let sending = $state(false);
	let autoSend = $state(false);
	let stickyModifiers = $state<number[]>([]);
	let physicalModifiers = $state<number[]>([]);
	let keyboardMode = $state<Language>('pl');
	let isCapsLocked = $state(false);
	let captureMode = $state(false);
	let lastStatus = $state('');
	let lastStatusKind = $state<StatusKind>('');

	let statusClearTimeout: ReturnType<typeof setTimeout> | undefined;
	let inputClearTimeout: ReturnType<typeof setTimeout> | undefined;
	let altGrReplaceTimeout: ReturnType<typeof setTimeout> | undefined;
	let statusVersion = 0;
	let managerInitVersion = 0;
	let captureListenersAttached = false;

	const pressedPhysicalButtons = new Map<string, string>();
	const pressedPhysicalModifiers = new Map<string, number>();

	$effect(() => {
		if (!manager) return;

		manager.setMode(keyboardMode);
		manager.setModifiers(getEffectiveModifiers());
		manager.setCapsLock(isCapsLocked);
		manager.setPressedButtons([...pressedPhysicalButtons.values()]);
		manager.setInput(textQueue);
	});

	function getEffectiveModifiers() {
		return Array.from(new Set([...stickyModifiers, ...physicalModifiers]));
	}

	function createApi(): KeyboardApi {
		if (deps.createApi) {
			return deps.createApi();
		}

		const options = apiClient?.options ?? {
			bearerToken: '',
			onUnauthorized: undefined
		};

		return new KeyboardApiService({
			...options,
			bearerToken: getToken() || options.bearerToken
		});
	}

	function attachManager(nextManager: KeyboardManagerAdapter, initVersion: number) {
		if (initVersion !== managerInitVersion) {
			nextManager.destroy();
			return;
		}

		manager = nextManager;
		manager.setMode(keyboardMode);
		manager.setModifiers(getEffectiveModifiers());
		manager.setCapsLock(isCapsLocked);
		manager.setPressedButtons([...pressedPhysicalButtons.values()]);
		manager.setInput(textQueue);
	}

	function isPromiseLike<T>(value: T | Promise<T>): value is Promise<T> {
		return typeof (value as Promise<T> | undefined)?.then === 'function';
	}

	function init() {
		managerInitVersion += 1;
		const initVersion = managerInitVersion;
		manager?.destroy();
		manager = undefined;

		const nextManager =
			deps.createManager?.(
				(input) => onChange(input),
				(button) => onKeyPress(button)
			) ??
			createSimpleKeyboardManager(
				(input) => onChange(input),
				(button) => onKeyPress(button)
			);

		if (isPromiseLike(nextManager)) {
			void nextManager
				.then((resolvedManager) => {
					attachManager(resolvedManager, initVersion);
				})
				.catch((error) => {
					if (initVersion !== managerInitVersion) return;
					console.error(error);
					setLastStatus(m.keyboard_network_error(), 'error', 3000);
				});
			return;
		}

		attachManager(nextManager, initVersion);
	}

	function destroy() {
		clearTransientTimeouts();
		clearStatusTimeout();
		detachCaptureListeners();
		releaseTrackedPhysicalKeys();
		captureMode = false;
		managerInitVersion += 1;
		manager?.destroy();
		manager = undefined;
	}

	function onChange(input: string) {
		textQueue = input;
	}

	function onKeyPress(button: string) {
		if (button === '{ctrl}') {
			toggleModifier(KEY_LEFT_CTRL);
			return;
		}
		if (button === '{alt}') {
			toggleModifier(KEY_LEFT_ALT);
			return;
		}
		if (button === '{win}') {
			toggleModifier(KEY_LEFT_GUI);
			return;
		}
		if (button === '{altgr}') {
			toggleModifier(KEY_RIGHT_ALT);
			return;
		}

		if (button === '{shift}') {
			toggleModifier(KEY_LEFT_SHIFT);
			return;
		}
		if (button === '{lock}') {
			isCapsLocked = !isCapsLocked;
			return;
		}

		if (SPECIAL_KEYS[button]) {
			void pressDirect(SPECIAL_KEYS[button]);
			return;
		}

		const isSpace = button === '{space}';
		const isNormal = button.length === 1 || isSpace;
		const effectiveModifiers = getEffectiveModifiers();

		const hasComboMod = effectiveModifiers.some(
			(modifier) =>
				modifier === KEY_LEFT_CTRL || modifier === KEY_LEFT_ALT || modifier === KEY_LEFT_GUI
		);

		if (hasComboMod && isNormal) {
			const charCode = isSpace ? 32 : button.charCodeAt(0);
			void pressDirect(charCode);
			return;
		}

		if (autoSend && isNormal) {
			let char = isSpace ? ' ' : button;
			const config = LAYOUT_CONFIG[keyboardMode];

			if (effectiveModifiers.includes(KEY_RIGHT_ALT) && config.altGrMap[char]) {
				char = config.altGrMap[char];
			}

			void sendImmediate(char);
			scheduleInputClear();
		}

		if (
			effectiveModifiers.includes(KEY_LEFT_SHIFT) &&
			!isCapsLocked &&
			button !== '{shift}' &&
			button !== '{lock}'
		) {
			stickyModifiers = stickyModifiers.filter((modifier) => modifier !== KEY_LEFT_SHIFT);
		}

		if (effectiveModifiers.includes(KEY_RIGHT_ALT) && isNormal && button !== '{altgr}') {
			stickyModifiers = stickyModifiers.filter((modifier) => modifier !== KEY_RIGHT_ALT);
		}

		if (!autoSend && isNormal && effectiveModifiers.includes(KEY_RIGHT_ALT)) {
			const char = isSpace ? ' ' : button;
			const config = LAYOUT_CONFIG[keyboardMode];

			if (config.altGrMap[char]) {
				scheduleAltGrReplacement(button, config.altGrMap[button]);
			}
		}
	}

	async function sendImmediate(text: string) {
		if (!autoSend) return;

		try {
			await createApi().typeText({ text, enter: false });
			setLastStatus(m.keyboard_sent(), 'success', 500);
		} catch (error) {
			console.error(error);
			setLastStatus(toKeyboardErrorMessage(error), 'error', 3000);
		}
	}

	async function sendText() {
		if (!textQueue || sending) return;

		sending = true;
		setLastStatus(m.keyboard_sending(), 'info');

		try {
			await createApi().typeText({ text: textQueue, enter: true });
			textQueue = '';
			manager?.clearInput();
			setLastStatus(m.keyboard_sent(), 'success', 2000);
		} catch (error) {
			console.error(error);
			setLastStatus(toKeyboardErrorMessage(error), 'error');
		} finally {
			sending = false;
		}
	}

	async function pressDirect(key: number) {
		try {
			const effectiveModifiers = getEffectiveModifiers();
			const payload =
				effectiveModifiers.length > 0 ? { keys: [...effectiveModifiers, key] } : { key };

			await createApi().pressKey(payload);
			setLastStatus(m.keyboard_sent(), 'success', 1000);

			if (effectiveModifiers.length > 0) {
				clearModifiers();
			}
		} catch (error) {
			console.error(error);
			setLastStatus(toKeyboardErrorMessage(error), 'error', 3000);
		}
	}

	function toggleModifier(modifier: number) {
		if (stickyModifiers.includes(modifier)) {
			stickyModifiers = stickyModifiers.filter((value) => value !== modifier);
			return;
		}

		stickyModifiers = [...stickyModifiers, modifier];
	}

	function clearModifiers() {
		stickyModifiers = [];
	}

	function toggleLanguage() {
		if (keyboardMode === 'pl') keyboardMode = 'en';
		else if (keyboardMode === 'en') keyboardMode = 'ru';
		else keyboardMode = 'pl';

		clearModifiers();
	}

	function toggleLiveMode() {
		autoSend = !autoSend;
	}

	function refreshPhysicalKeyboardHighlight() {
		manager?.setPressedButtons([...pressedPhysicalButtons.values()]);
	}

	function syncPhysicalModifiers() {
		physicalModifiers = Array.from(new Set([...pressedPhysicalModifiers.values()]));
	}

	function releaseTrackedPhysicalKeys() {
		pressedPhysicalButtons.clear();
		pressedPhysicalModifiers.clear();
		physicalModifiers = [];
		refreshPhysicalKeyboardHighlight();
	}

	function attachCaptureListeners() {
		if (
			captureListenersAttached ||
			typeof window === 'undefined' ||
			typeof document === 'undefined'
		) {
			return;
		}

		window.addEventListener('keydown', handlePhysicalKeydown, true);
		window.addEventListener('keyup', handlePhysicalKeyup, true);
		window.addEventListener('blur', handleCaptureWindowBlur);
		document.addEventListener('visibilitychange', handleVisibilityChange);
		captureListenersAttached = true;
	}

	function detachCaptureListeners() {
		if (
			!captureListenersAttached ||
			typeof window === 'undefined' ||
			typeof document === 'undefined'
		) {
			return;
		}

		window.removeEventListener('keydown', handlePhysicalKeydown, true);
		window.removeEventListener('keyup', handlePhysicalKeyup, true);
		window.removeEventListener('blur', handleCaptureWindowBlur);
		document.removeEventListener('visibilitychange', handleVisibilityChange);
		captureListenersAttached = false;
	}

	function handleCaptureWindowBlur() {
		releaseTrackedPhysicalKeys();
	}

	function handleVisibilityChange() {
		if (typeof document !== 'undefined' && document.hidden) {
			releaseTrackedPhysicalKeys();
		}
	}

	function syncCapsLockState(event: Pick<KeyboardEvent, 'getModifierState'>) {
		if (typeof event.getModifierState === 'function') {
			isCapsLocked = event.getModifierState('CapsLock');
		}
	}

	function handlePhysicalKeydown(event: KeyboardEvent) {
		if (!captureMode || event.isComposing || event.key === 'Dead') {
			return;
		}

		const buttonName = getPhysicalKeyboardButton(event);
		const modifier = getPhysicalKeyboardModifier(event.code);

		if (!buttonName && modifier === null && !isCapsLockCode(event.code)) {
			return;
		}

		if (pressedPhysicalButtons.has(event.code) || pressedPhysicalModifiers.has(event.code)) {
			event.preventDefault();
			return;
		}

		event.preventDefault();

		if (buttonName) {
			pressedPhysicalButtons.set(event.code, buttonName);
			refreshPhysicalKeyboardHighlight();
		}

		if (modifier !== null) {
			pressedPhysicalModifiers.set(event.code, modifier);
			syncPhysicalModifiers();
		}

		if (isCapsLockCode(event.code) || typeof event.getModifierState === 'function') {
			syncCapsLockState(event);
		}
	}

	function handlePhysicalKeyup(event: KeyboardEvent) {
		if (!captureMode) {
			return;
		}

		const hadButton = pressedPhysicalButtons.delete(event.code);
		const hadModifier = pressedPhysicalModifiers.delete(event.code);

		if (!hadButton && !hadModifier) {
			return;
		}

		event.preventDefault();
		syncCapsLockState(event);
		syncPhysicalModifiers();
		refreshPhysicalKeyboardHighlight();
	}

	function toggleCaptureMode() {
		if (captureMode) {
			detachCaptureListeners();
			captureMode = false;
			releaseTrackedPhysicalKeys();
			return;
		}

		clearModifiers();
		captureMode = true;
		attachCaptureListeners();
	}

	function handleKeydown(event: KeyboardEvent) {
		if (captureMode) {
			return;
		}

		if (event.key === 'Enter' && !event.shiftKey) {
			event.preventDefault();
			void sendText();
		}
	}

	function setLastStatus(status: string, kind: StatusKind = 'info', clearAfterMs?: number) {
		statusVersion += 1;
		const currentVersion = statusVersion;

		clearStatusTimeout();

		lastStatus = status;
		lastStatusKind = status ? kind : '';

		if (!clearAfterMs) return;

		statusClearTimeout = setTimeout(() => {
			if (statusVersion !== currentVersion) return;
			setLastStatus('');
		}, clearAfterMs);
	}

	function clearStatusTimeout() {
		if (!statusClearTimeout) return;
		clearTimeout(statusClearTimeout);
		statusClearTimeout = undefined;
	}

	function clearTransientTimeouts() {
		if (inputClearTimeout) {
			clearTimeout(inputClearTimeout);
			inputClearTimeout = undefined;
		}

		if (altGrReplaceTimeout) {
			clearTimeout(altGrReplaceTimeout);
			altGrReplaceTimeout = undefined;
		}
	}

	function scheduleInputClear() {
		if (inputClearTimeout) {
			clearTimeout(inputClearTimeout);
		}

		inputClearTimeout = setTimeout(() => {
			manager?.clearInput();
			textQueue = '';
			inputClearTimeout = undefined;
		}, 0);
	}

	function scheduleAltGrReplacement(button: string, mappedChar: string) {
		if (altGrReplaceTimeout) {
			clearTimeout(altGrReplaceTimeout);
		}

		altGrReplaceTimeout = setTimeout(() => {
			const current = manager?.keyboard?.getInput() || '';
			if (current.endsWith(button)) {
				const newValue = current.slice(0, -1) + mappedChar;
				manager?.setInput(newValue);
				textQueue = newValue;
			}
			altGrReplaceTimeout = undefined;
		}, 10);
	}

	function toKeyboardErrorMessage(error: unknown) {
		if (typeof error === 'object' && error !== null && 'errorCode' in error) {
			const { errorCode } = error as { errorCode?: unknown };
			if (errorCode === 'keyboard/disabled') {
				return m.keyboard_error_disabled();
			}
		}

		if (typeof error === 'object' && error !== null && 'status' in error) {
			const { status } = error as { status?: unknown };
			if (typeof status === 'number') {
				return m.keyboard_error_status({ status });
			}
		}

		return m.keyboard_network_error();
	}

	return {
		get manager() {
			return manager;
		},
		get textQueue() {
			return textQueue;
		},
		set textQueue(value: string) {
			textQueue = value;
		},
		get sending() {
			return sending;
		},
		get autoSend() {
			return autoSend;
		},
		get activeModifiers() {
			return getEffectiveModifiers();
		},
		get keyboardMode() {
			return keyboardMode;
		},
		get isCapsLocked() {
			return isCapsLocked;
		},
		get captureMode() {
			return captureMode;
		},
		get lastStatus() {
			return lastStatus;
		},
		get lastStatusKind() {
			return lastStatusKind;
		},
		init,
		destroy,
		onChange,
		onKeyPress,
		sendImmediate,
		sendText,
		pressDirect,
		toggleModifier,
		clearModifiers,
		toggleLanguage,
		toggleLiveMode,
		toggleCaptureMode,
		handleKeydown,
		setLastStatus
	};
}
