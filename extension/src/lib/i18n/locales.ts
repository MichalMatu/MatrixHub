import { browser } from "wxt/browser";

export const SUPPORTED_LOCALES = ["en", "pl"] as const;
export type SupportedLocale = (typeof SUPPORTED_LOCALES)[number];

export type LocalePreference = SupportedLocale | "auto";

export const DEFAULT_LOCALE: SupportedLocale = "en";

const LOCALE_PREFERENCE_STORAGE_KEY = "matrixhub-sidepanel-locale";
const localeSet = new Set<string>(SUPPORTED_LOCALES);

function normalizeLanguageTag(input?: string | null) {
  return input?.replace(/_/g, "-").toLowerCase() ?? "";
}

export function resolveSupportedLocale(
  input?: string | null,
): SupportedLocale | null {
  const normalized = normalizeLanguageTag(input);
  if (!normalized) {
    return null;
  }

  if (localeSet.has(normalized)) {
    return normalized as SupportedLocale;
  }

  const language = normalized.split("-")[0];
  if (localeSet.has(language)) {
    return language as SupportedLocale;
  }

  return null;
}

export function detectBrowserLocale(): SupportedLocale {
  try {
    return (
      resolveSupportedLocale(browser.i18n?.getUILanguage()) ??
      resolveSupportedLocale(
        typeof navigator !== "undefined" ? navigator.language : null,
      ) ??
      DEFAULT_LOCALE
    );
  } catch {
    return DEFAULT_LOCALE;
  }
}

export function normalizeLocalePreference(input?: string | null) {
  return resolveSupportedLocale(input) ?? "auto";
}

export function resolveLocalePreference(
  preference: LocalePreference,
  browserLocale = detectBrowserLocale(),
): SupportedLocale {
  return preference === "auto" ? browserLocale : preference;
}

export function readStoredLocalePreference(): LocalePreference {
  if (typeof localStorage === "undefined") {
    return "auto";
  }

  try {
    return normalizeLocalePreference(
      localStorage.getItem(LOCALE_PREFERENCE_STORAGE_KEY),
    );
  } catch {
    return "auto";
  }
}

export function persistLocalePreference(preference: LocalePreference) {
  if (typeof localStorage === "undefined") {
    return;
  }

  try {
    if (preference === "auto") {
      localStorage.removeItem(LOCALE_PREFERENCE_STORAGE_KEY);
      return;
    }

    localStorage.setItem(LOCALE_PREFERENCE_STORAGE_KEY, preference);
  } catch {
    // Ignore storage write failures in extension pages.
  }
}
