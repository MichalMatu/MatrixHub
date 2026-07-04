import {
	KEY_LEFT_ALT,
	KEY_LEFT_CTRL,
	KEY_LEFT_GUI,
	KEY_LEFT_SHIFT,
	KEY_RIGHT_ALT
} from './constants';

const DIRECT_BUTTON_MAP: Record<string, string> = {
	Escape: '{esc}',
	Backspace: '{bksp}',
	Tab: '{tab}',
	Enter: '{enter}',
	NumpadEnter: '{enter}',
	Delete: '{del}',
	ArrowUp: '{arrowup}',
	ArrowDown: '{arrowdown}',
	ArrowLeft: '{arrowleft}',
	ArrowRight: '{arrowright}',
	PageUp: '{pgup}',
	PageDown: '{pgdn}',
	Home: '{home}',
	End: '{end}',
	F1: '{f1}',
	F2: '{f2}',
	F3: '{f3}',
	F4: '{f4}',
	F5: '{f5}',
	F6: '{f6}',
	F7: '{f7}',
	F8: '{f8}',
	F9: '{f9}',
	F10: '{f10}',
	F11: '{f11}',
	F12: '{f12}',
	ShiftLeft: '{shift}',
	ShiftRight: '{shift}',
	ControlLeft: '{ctrl}',
	ControlRight: '{ctrl}',
	AltLeft: '{alt}',
	AltRight: '{altgr}',
	MetaLeft: '{win}',
	MetaRight: '{win}',
	CapsLock: '{lock}',
	Space: '{space}'
};

const DIRECT_MODIFIER_MAP: Record<string, number> = {
	ShiftLeft: KEY_LEFT_SHIFT,
	ShiftRight: KEY_LEFT_SHIFT,
	ControlLeft: KEY_LEFT_CTRL,
	ControlRight: KEY_LEFT_CTRL,
	AltLeft: KEY_LEFT_ALT,
	AltRight: KEY_RIGHT_ALT,
	MetaLeft: KEY_LEFT_GUI,
	MetaRight: KEY_LEFT_GUI
};

export function getPhysicalKeyboardButton(event: Pick<KeyboardEvent, 'code' | 'key'>) {
	const directButton = DIRECT_BUTTON_MAP[event.code];
	if (directButton) {
		return directButton;
	}

	if (typeof event.key === 'string' && event.key.length === 1) {
		return event.key;
	}

	return null;
}

export function getPhysicalKeyboardModifier(code: string) {
	return DIRECT_MODIFIER_MAP[code] ?? null;
}

export function isCapsLockCode(code: string) {
	return code === 'CapsLock';
}
