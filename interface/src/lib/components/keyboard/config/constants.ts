// Modifiers
export const KEY_LEFT_CTRL = 0x80;
export const KEY_LEFT_SHIFT = 0x81;
export const KEY_LEFT_ALT = 0x82;
export const KEY_LEFT_GUI = 0x83; // Win/Cmd
export const KEY_RIGHT_ALT = 0x86; // Used internally for AltGr

// Key Mappings for Direct Send (Non-text keys)
export const SPECIAL_KEYS: Record<string, number> = {
	'{f1}': 194,
	'{f2}': 195,
	'{f3}': 196,
	'{f4}': 197,
	'{f5}': 198,
	'{f6}': 199,
	'{f7}': 200,
	'{f8}': 201,
	'{f9}': 202,
	'{f10}': 203,
	'{f11}': 204,
	'{f12}': 205,
	'{esc}': 177,
	'{bksp}': 178,
	'{tab}': 179,
	'{enter}': 176,
	'{del}': 212,
	'{arrowup}': 218,
	'{arrowdown}': 217,
	'{arrowleft}': 216,
	'{arrowright}': 215,
	'{pgup}': 211,
	'{pgdn}': 214,
	'{home}': 210,
	'{end}': 213,
	'{ctrl}': KEY_LEFT_CTRL,
	'{shift}': KEY_LEFT_SHIFT,
	'{alt}': KEY_LEFT_ALT,
	'{win}': KEY_LEFT_GUI,
	'{altgr}': KEY_RIGHT_ALT,
	// Media Keys (Custom Mapped in Backend)
	'{play}': 233,
	'{prev}': 231,
	'{next}': 230,
	'{mute}': 234,
	'{volup}': 235,
	'{voldn}': 236
};
