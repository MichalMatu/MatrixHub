export type Language = 'pl' | 'en' | 'ru';

export interface KeyboardConfig {
	name: string;
	altGrMap: Record<string, string>;
	layoutBase: string; // 'default' or others
	layoutShift: string; // 'shift' or others
	layoutAltGr?: string; // Visual layout for AltGr
	layoutAltGrShift?: string; // Visual layout for AltGr+Shift
}
