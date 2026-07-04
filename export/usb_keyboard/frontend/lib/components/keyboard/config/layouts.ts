import type { Language, KeyboardConfig } from './types';

const polishMap: Record<string, string> = {
	a: 'ą',
	c: 'ć',
	e: 'ę',
	l: 'ł',
	n: 'ń',
	o: 'ó',
	s: 'ś',
	z: 'ż',
	x: 'ź',
	A: 'Ą',
	C: 'Ć',
	E: 'Ę',
	L: 'Ł',
	N: 'Ń',
	O: 'Ó',
	S: 'Ś',
	Z: 'Ż',
	X: 'Ź'
};

export const LAYOUT_CONFIG: Record<Language, KeyboardConfig> = {
	pl: {
		name: 'PL',
		altGrMap: polishMap,
		layoutBase: 'default',
		layoutShift: 'shift',
		layoutAltGr: 'polish',
		layoutAltGrShift: 'polishShift'
	},
	en: {
		name: 'EN',
		altGrMap: {},
		layoutBase: 'default',
		layoutShift: 'shift',
		layoutAltGr: undefined, // No special layout
		layoutAltGrShift: undefined
	},
	ru: {
		name: 'RU',
		altGrMap: {},
		layoutBase: 'russian', // To be defined in layout definitions
		layoutShift: 'russianShift'
	}
};

export const KEYBOARD_LAYOUTS = {
	default: [
		'{home} {end} {pgup} {pgdn} {prev} {play} {next} {mute} {voldn} {volup}',
		'{esc} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}',
		'` 1 2 3 4 5 6 7 8 9 0 - = {bksp}',
		'{tab} q w e r t y u i o p [ ] \\ {del}',
		"{lock} a s d f g h j k l ; ' {enter}",
		'{shift} z x c v b n m , . / {shift}',
		'{ctrl} {alt} {win} {space} {altgr} {alt} {arrowleft} {arrowup} {arrowdown} {arrowright}'
	],
	shift: [
		'{home} {end} {pgup} {pgdn} {prev} {play} {next} {mute} {voldn} {volup}',
		'{esc} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}',
		'~ ! @ # $ % ^ & * ( ) _ + {bksp}',
		'{tab} Q W E R T Y U I O P { } | {del}',
		'{lock} A S D F G H J K L : " {enter}',
		'{shift} Z X C V B N M < > ? {shift}',
		'{ctrl} {alt} {win} {space} {altgr} {alt} {arrowleft} {arrowup} {arrowdown} {arrowright}'
	],
	polish: [
		'{home} {end} {pgup} {pgdn} {prev} {play} {next} {mute} {voldn} {volup}',
		'{esc} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}',
		'` 1 2 3 4 5 6 7 8 9 0 - = {bksp}',
		'{tab} q w ę r t y u i ó p [ ] \\ {del}',
		"{lock} ą ś d f g h j k ł ; ' {enter}",
		'{shift} ż ź ć v b ń m , . / {shift}',
		'{ctrl} {alt} {win} {space} {altgr} {alt} {arrowleft} {arrowup} {arrowdown} {arrowright}'
	],
	polishShift: [
		'{home} {end} {pgup} {pgdn} {prev} {play} {next} {mute} {voldn} {volup}',
		'{esc} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}',
		'~ ! @ # $ % ^ & * ( ) _ + {bksp}',
		'{tab} Q W Ę R T Y U I Ó P { } | {del}',
		'{lock} Ą Ś D F G H J K Ł : " {enter}',
		'{shift} Ż Ź Ć V B Ń M < > ? {shift}',
		'{ctrl} {alt} {win} {space} {altgr} {alt} {arrowleft} {arrowup} {arrowdown} {arrowright}'
	],
	russian: [
		'{home} {end} {pgup} {pgdn} {prev} {play} {next} {mute} {voldn} {volup}',
		'{esc} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}',
		'ё 1 2 3 4 5 6 7 8 9 0 - = {bksp}',
		'{tab} й ц у к е н г ш щ з х ъ \\ {del}',
		'{lock} ф ы в а п р о л д ж э {enter}',
		'{shift} я ч с м и т ь б ю . {shift}',
		'{ctrl} {alt} {win} {space} {altgr} {alt} {arrowleft} {arrowup} {arrowdown} {arrowright}'
	],
	russianShift: [
		'{home} {end} {pgup} {pgdn} {prev} {play} {next} {mute} {voldn} {volup}',
		'{esc} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}',
		'Ё ! " № ; % : ? * ( ) _ + {bksp}',
		'{tab} Й Ц У К Е Н Г Ш Щ З Х Ъ / {del}',
		'{lock} Ф Ы В А П Р О Л Д Ж Э {enter}',
		'{shift} Я Ч С М И Т Ь Б Ю , {shift}',
		'{ctrl} {alt} {win} {space} {altgr} {alt} {arrowleft} {arrowup} {arrowdown} {arrowright}'
	]
};

export const KEY_DISPLAY = {
	'{esc}': 'Esc',
	'{f1}': 'F1',
	'{f2}': 'F2',
	'{f3}': 'F3',
	'{f4}': 'F4',
	'{f5}': 'F5',
	'{f6}': 'F6',
	'{f7}': 'F7',
	'{f8}': 'F8',
	'{f9}': 'F9',
	'{f10}': 'F10',
	'{f11}': 'F11',
	'{f12}': 'F12',
	'{bksp}': '⌫',
	'{tab}': 'Tab',
	'{lock}': 'Caps',
	'{enter}': 'Enter',
	'{shift}': '⇧',
	'{space}': ' ',
	'{arrowup}': '↑',
	'{arrowdown}': '↓',
	'{arrowleft}': '←',
	'{arrowright}': '→',
	'{ctrl}': 'Ctrl',
	'{alt}': 'Option',
	'{win}': 'Cmd',
	'{del}': 'Del',
	'{altgr}': 'AltGr',
	// New Keys
	'{home}': 'Home',
	'{end}': 'End',
	'{pgup}': 'PgUp',
	'{pgdn}': 'PgDn',
	'{play}': '⏯',
	'{prev}': '⏮',
	'{next}': '⏭',
	'{mute}': '🔇',
	'{volup}': '🔊',
	'{voldn}': '🔉'
};
