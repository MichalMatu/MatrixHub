import { browser } from '$app/environment';

export const AVAILABLE_THEMES = [
	'business',
	'corporate',
	'night',
	'black',
	'luxury',
	'dracula',
	'forest',
	'coffee',
	'dim',
	'sunset',
	'halloween',
	'synthwave',
	'light',
	'nord',
	'retro'
] as const;

type ThemeName = (typeof AVAILABLE_THEMES)[number];

interface ThemeSettings {
	theme: string;
	borderRadius: string; // e.g., '0.5rem'
	primaryColor: string; // e.g., '#3b82f6'
	animationSpeed: string; // e.g., '0.5s'
	fontSize?: string; // e.g. '100%'
	borderWidth?: string; // e.g. '1px'
	focusScale?: string; // e.g. '0.95'
}

const DEFAULT_THEME_SETTINGS: ThemeSettings = {
	theme: 'business',
	borderRadius: '0.25rem', // Default for business
	primaryColor: '', // Empty means use theme default
	animationSpeed: '0.2s',
	fontSize: '100%',
	borderWidth: '1px',
	focusScale: '0.95'
};

const THEME_STORAGE_KEY = 'theme_settings';

function cloneSettings(settings: ThemeSettings): ThemeSettings {
	return { ...settings };
}

function serializeSettings(settings: ThemeSettings): string {
	return JSON.stringify(settings);
}

function normalizeSettings(candidate: Partial<ThemeSettings> | null | undefined): ThemeSettings {
	const normalized = { ...DEFAULT_THEME_SETTINGS, ...(candidate ?? {}) };
	if (!AVAILABLE_THEMES.includes(normalized.theme as ThemeName)) {
		normalized.theme = DEFAULT_THEME_SETTINGS.theme;
	}
	return normalized;
}

function loadStoredSettings(): ThemeSettings {
	if (!browser) {
		return cloneSettings(DEFAULT_THEME_SETTINGS);
	}

	const stored = localStorage.getItem(THEME_STORAGE_KEY);
	if (!stored) {
		return cloneSettings(DEFAULT_THEME_SETTINGS);
	}

	try {
		const parsed = JSON.parse(stored) as Partial<ThemeSettings>;
		return normalizeSettings(parsed);
	} catch {
		return cloneSettings(DEFAULT_THEME_SETTINGS);
	}
}

class ThemeStore {
	settings = $state<ThemeSettings>(cloneSettings(DEFAULT_THEME_SETTINGS));
	private savedSettings = $state<ThemeSettings>(cloneSettings(DEFAULT_THEME_SETTINGS));
	private skipInitialEffect = true;

	constructor() {
		if (browser) {
			const initialSettings = loadStoredSettings();
			this.settings = cloneSettings(initialSettings);
			this.savedSettings = cloneSettings(initialSettings);
			this.apply(initialSettings);
		}

		$effect.root(() => {
			$effect(() => {
				const settings = $state.snapshot(this.settings);
				if (this.skipInitialEffect) {
					this.skipInitialEffect = false;
					return;
				}

				this.apply(settings);
			});
		});
	}

	get hasChanges() {
		return serializeSettings(this.settings) !== serializeSettings(this.savedSettings);
	}

	persist(settings: ThemeSettings = this.settings) {
		if (browser) {
			localStorage.setItem(THEME_STORAGE_KEY, serializeSettings(settings));
		}
	}

	apply(settings: ThemeSettings = this.settings) {
		if (!browser) return;
		const html = document.documentElement;
		const root = document.documentElement;

		// Apply Theme
		html.setAttribute('data-theme', settings.theme);

		// Layout / Typography
		html.style.fontSize = settings.fontSize || '100%';

		// Roundness
		// DaisyUI v5 uses --radius-* variables
		const r = settings.borderRadius;
		root.style.setProperty('--radius-box', r);
		root.style.setProperty('--radius-btn', r);
		root.style.setProperty('--radius-field', r);
		root.style.setProperty('--radius-selector', r);

		// Tailwind v4 global radius
		root.style.setProperty('--radius', r);

		// Animation
		root.style.setProperty('--animation-btn', settings.animationSpeed);
		root.style.setProperty('--animation-input', settings.animationSpeed);

		// Borders & Effects
		root.style.setProperty('--border-btn', settings.borderWidth || '1px');
		root.style.setProperty('--btn-focus-scale', settings.focusScale || '0.95');
	}

	setTheme(theme: string) {
		this.settings.theme = theme;
	}

	setRadius(radius: string) {
		this.settings.borderRadius = radius;
	}

	setAnimationSpeed(speed: string) {
		this.settings.animationSpeed = speed;
	}

	setFontSize(size: string) {
		this.settings.fontSize = size;
	}

	setBorderWidth(width: string) {
		this.settings.borderWidth = width;
	}

	setFocusScale(scale: string) {
		this.settings.focusScale = scale;
	}

	save() {
		const snapshot = cloneSettings($state.snapshot(this.settings));
		this.savedSettings = cloneSettings(snapshot);
		this.persist(snapshot);
	}

	reset() {
		const snapshot = cloneSettings($state.snapshot(this.savedSettings));
		this.settings = cloneSettings(snapshot);
		this.apply(snapshot);
	}
}

export const themeStore = new ThemeStore();
