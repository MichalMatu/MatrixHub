import type { TranslationKey } from "./catalog";
import { resolveMessageValue } from "./catalog";
import type { LocalePreference, SupportedLocale } from "./locales";
import {
  persistLocalePreference,
  readStoredLocalePreference,
  resolveLocalePreference,
} from "./locales";
import { DEFAULT_LOCALE, detectBrowserLocale } from "./locales";
import type { TranslationParams } from "./types";

export interface I18nRuntime {
  readonly locale: SupportedLocale;
  readonly preference: LocalePreference;
  t(key: TranslationKey, params?: TranslationParams): string;
  formatInteger(value: number): string;
  formatDecimal(value: number, fractionDigits?: number): string;
}

function interpolate(template: string, params: TranslationParams = {}) {
  return template.replace(/\{(\w+)\}/g, (_, name: string) => {
    const value = params[name];
    return value === null || value === undefined ? `{${name}}` : String(value);
  });
}

export function createI18nRuntime(
  locale: SupportedLocale,
  preference: LocalePreference = locale,
): I18nRuntime {
  const pluralRules = new Intl.PluralRules(locale);
  const integerFormatter = new Intl.NumberFormat(locale, {
    maximumFractionDigits: 0,
  });
  const decimalFormatters = new Map<number, Intl.NumberFormat>();

  function getDecimalFormatter(fractionDigits: number) {
    const existing = decimalFormatters.get(fractionDigits);
    if (existing) {
      return existing;
    }

    const created = new Intl.NumberFormat(locale, {
      minimumFractionDigits: fractionDigits,
      maximumFractionDigits: fractionDigits,
    });
    decimalFormatters.set(fractionDigits, created);
    return created;
  }

  function t(key: TranslationKey, params: TranslationParams = {}) {
    const resolved =
      resolveMessageValue(locale, key) ??
      resolveMessageValue(DEFAULT_LOCALE, key);

    if (!resolved) {
      return key;
    }

    if (typeof resolved === "string") {
      return interpolate(resolved, params);
    }

    const rawCount = params.count;
    const count =
      typeof rawCount === "number"
        ? rawCount
        : typeof rawCount === "string"
          ? Number(rawCount)
          : Number.NaN;

    const category = Number.isFinite(count)
      ? pluralRules.select(count)
      : "other";
    const template = resolved[category] ?? resolved.other;
    return interpolate(template, params);
  }

  return {
    locale,
    preference,
    t,
    formatInteger(value: number) {
      return integerFormatter.format(value);
    },
    formatDecimal(value: number, fractionDigits = 1) {
      return getDecimalFormatter(fractionDigits).format(value);
    },
  };
}

let currentPreference = readStoredLocalePreference();
let currentRuntime = createI18nRuntime(
  resolveLocalePreference(currentPreference),
  currentPreference,
);

const listeners = new Set<(runtime: I18nRuntime) => void>();

function notifyListeners() {
  listeners.forEach((listener) => listener(currentRuntime));
}

export function getI18nRuntime() {
  return currentRuntime;
}

export function subscribeI18n(listener: (runtime: I18nRuntime) => void) {
  listeners.add(listener);
  listener(currentRuntime);

  return () => {
    listeners.delete(listener);
  };
}

export function setLocalePreference(preference: LocalePreference) {
  currentPreference = preference;
  persistLocalePreference(preference);
  currentRuntime = createI18nRuntime(
    resolveLocalePreference(preference),
    preference,
  );
  notifyListeners();
  return currentRuntime;
}

export function refreshLocaleFromBrowser() {
  if (currentPreference !== "auto") {
    return currentRuntime;
  }

  currentRuntime = createI18nRuntime(detectBrowserLocale(), currentPreference);
  notifyListeners();
  return currentRuntime;
}

export function t(key: TranslationKey, params?: TranslationParams) {
  return currentRuntime.t(key, params);
}
