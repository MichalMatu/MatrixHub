import Keyboard from 'simple-keyboard';
import 'simple-keyboard/build/css/index.css';
import {
	KEY_LEFT_CTRL,
	KEY_LEFT_SHIFT,
	KEY_LEFT_ALT,
	KEY_LEFT_GUI,
	KEY_RIGHT_ALT
} from '../config/constants';
import { LAYOUT_CONFIG, KEYBOARD_LAYOUTS, KEY_DISPLAY } from '../config/layouts';
import type { Language } from '../config/types';

interface KeyboardInstance {
	getInput(): string;
	setInput(input: string): void;
	clearInput(): void;
	setOptions(options: Record<string, unknown>): void;
}

type KeyboardConstructor = new (options: Record<string, unknown>) => KeyboardInstance;

class SimpleKeyboardManager {
	keyboard: KeyboardInstance | null = null;
	mode: Language = 'pl';
	activeModifiers: number[] = [];
	isCapsLocked: boolean = false;
	pressedButtons: string[] = [];

	constructor(
		Keyboard: KeyboardConstructor,
		onChange: (input: string) => void,
		onKeyPress: (button: string) => void
	) {
		this.keyboard = new Keyboard({
			onChange,
			onKeyPress,
			theme: 'hg-theme-default my-dark-theme',
			layout: KEYBOARD_LAYOUTS,
			display: KEY_DISPLAY,
			physicalKeyboardHighlight: false,
			useTouchEvents: false
		});
	}

	setMode(mode: Language) {
		this.mode = mode;
		this.updateLayout();
	}

	setInput(input: string) {
		if (this.keyboard && input !== this.keyboard.getInput()) {
			this.keyboard.setInput(input);
		}
	}

	clearInput() {
		this.keyboard?.clearInput();
	}

	setModifiers(modifiers: number[]) {
		this.activeModifiers = modifiers;
		this.updateLayout();
	}

	setCapsLock(isLocked: boolean) {
		this.isCapsLocked = isLocked;
		this.updateLayout();
	}

	setPressedButtons(buttons: string[]) {
		this.pressedButtons = buttons;
		this.updateButtonTheme();
	}

	updateLayout() {
		if (!this.keyboard) return;

		const config = LAYOUT_CONFIG[this.mode];
		const altGr = this.activeModifiers.includes(KEY_RIGHT_ALT);
		const shift = this.activeModifiers.includes(KEY_LEFT_SHIFT);

		let targetLayout = config.layoutBase;

		if (this.isCapsLocked || shift) {
			targetLayout = config.layoutShift;
		}

		if (altGr) {
			if ((this.isCapsLocked || shift) && config.layoutAltGrShift) {
				targetLayout = config.layoutAltGrShift;
			} else if (config.layoutAltGr) {
				targetLayout = config.layoutAltGr;
			}
		}

		this.keyboard.setOptions({ layoutName: targetLayout });
		this.updateButtonTheme();
	}

	updateButtonTheme() {
		if (!this.keyboard) return;

		let themeButtons = [];

		if (this.activeModifiers.includes(KEY_LEFT_CTRL)) themeButtons.push('{ctrl}');
		if (this.activeModifiers.includes(KEY_LEFT_ALT)) themeButtons.push('{alt}');
		if (this.activeModifiers.includes(KEY_LEFT_GUI)) themeButtons.push('{win}');
		if (this.activeModifiers.includes(KEY_LEFT_SHIFT)) themeButtons.push('{shift}');
		if (this.activeModifiers.includes(KEY_RIGHT_ALT)) themeButtons.push('{altgr}');

		if (this.isCapsLocked) themeButtons.push('{lock}');

		let buttonTheme = [];
		if (themeButtons.length > 0) {
			buttonTheme.push({
				class: 'key-active-dot',
				buttons: themeButtons.join(' ')
			});
		}

		if (this.pressedButtons.length > 0) {
			buttonTheme.push({
				class: 'key-capture-pressed',
				buttons: this.pressedButtons.join(' ')
			});
		}

		this.keyboard.setOptions({ buttonTheme });
	}

	destroy() {
		// Simple-keyboard doesn't have an explicit destroy, but we can clear references
		this.keyboard = null;
	}
}

export function createSimpleKeyboardManager(
	onChange: (input: string) => void,
	onKeyPress: (button: string) => void
) {
	return new SimpleKeyboardManager(
		Keyboard as unknown as KeyboardConstructor,
		onChange,
		onKeyPress
	);
}
