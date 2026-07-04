import { describe, it, expect } from 'vitest';
import { SPECIAL_KEYS } from './constants';

describe('Keyboard Key Codes', () => {
	it('should map Navigation keys to standard HID codes', () => {
		// HID Usage IDs (from USBHIDKeyboard.h)
		// KEY_HOME = 0xD2 (210)
		// KEY_PAGE_UP = 0xD3 (211)
		// KEY_END = 0xD5 (213)
		// KEY_PAGE_DOWN = 0xD6 (214)

		expect(SPECIAL_KEYS['{home}']).toBe(210);
		expect(SPECIAL_KEYS['{pgup}']).toBe(211);
		expect(SPECIAL_KEYS['{end}']).toBe(213);
		expect(SPECIAL_KEYS['{pgdn}']).toBe(214);
	});

	it('should map Media keys to designated frontend codes', () => {
		// These are custom codes mapped in KeyboardApiService.cpp -> HID Consumer Usage
		expect(SPECIAL_KEYS['{next}']).toBe(230);
		expect(SPECIAL_KEYS['{prev}']).toBe(231);
		// 232 skipped
		expect(SPECIAL_KEYS['{play}']).toBe(233);
		expect(SPECIAL_KEYS['{mute}']).toBe(234);
		expect(SPECIAL_KEYS['{volup}']).toBe(235);
		expect(SPECIAL_KEYS['{voldn}']).toBe(236);
	});
});
