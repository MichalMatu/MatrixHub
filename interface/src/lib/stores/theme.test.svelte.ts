// @vitest-environment jsdom
import { tick } from 'svelte';
import { beforeEach, describe, expect, it, vi } from 'vitest';

describe('ThemeStore', () => {
	beforeEach(() => {
		vi.resetModules();
		localStorage.clear();
		document.documentElement.removeAttribute('data-theme');
		document.documentElement.removeAttribute('style');
	});

	it('previews theme changes before saving and restores the saved draft', async () => {
		const { themeStore } = await import('./theme.svelte');

		expect(themeStore.hasChanges).toBe(false);
		expect(document.documentElement.getAttribute('data-theme')).toBe('business');

		themeStore.setTheme('night');
		themeStore.setRadius('1rem');
		await tick();

		expect(themeStore.hasChanges).toBe(true);
		expect(document.documentElement.getAttribute('data-theme')).toBe('night');
		expect(document.documentElement.style.getPropertyValue('--radius-box')).toBe('1rem');
		expect(localStorage.getItem('theme_settings')).toBeNull();

		themeStore.save();

		expect(themeStore.hasChanges).toBe(false);
		expect(localStorage.getItem('theme_settings')).toContain('"theme":"night"');

		themeStore.setTheme('forest');
		await tick();
		expect(themeStore.hasChanges).toBe(true);
		expect(document.documentElement.getAttribute('data-theme')).toBe('forest');

		themeStore.reset();
		await tick();

		expect(themeStore.hasChanges).toBe(false);
		expect(themeStore.settings.theme).toBe('night');
		expect(document.documentElement.getAttribute('data-theme')).toBe('night');
	});

	it('normalizes unknown stored themes to the default theme', async () => {
		localStorage.setItem(
			'theme_settings',
			JSON.stringify({
				theme: 'unknown-theme',
				borderRadius: '0.75rem'
			})
		);

		const { themeStore } = await import('./theme.svelte');

		expect(themeStore.settings.theme).toBe('business');
		expect(themeStore.settings.borderRadius).toBe('0.75rem');
		expect(document.documentElement.getAttribute('data-theme')).toBe('business');
	});
});
