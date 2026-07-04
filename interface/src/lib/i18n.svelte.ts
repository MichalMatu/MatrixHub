import * as runtime from '$lib/paraglide/runtime';
import { createPersistentLanguage } from './storage.svelte';

// Initialize storage defaulting to source language (usually 'en')
const langStore = createPersistentLanguage('user-lang', 'en');

// Explicitly define available languages matching project.inlang
type AvailableLanguageTag = 'en' | 'pl';

// Keep Paraglide runtime aligned with persisted language on first app load.
runtime.setLocale(langStore.value as AvailableLanguageTag);

export const i18n = {
	get languageTag() {
		return langStore.value as AvailableLanguageTag;
	},
	setLocale(newLocale: AvailableLanguageTag) {
		langStore.value = newLocale;
		// Update Paraglide runtime without reloading the page
		runtime.setLocale(newLocale);
	}
};
