import type { TranslationKey } from "$lib/i18n/catalog";

export const SIDEPANEL_THEME_NAMES = [
  "business",
  "corporate",
  "night",
  "black",
  "luxury",
  "dracula",
  "forest",
  "coffee",
  "dim",
  "sunset",
  "halloween",
  "synthwave",
  "light",
  "nord",
  "retro",
] as const;

export type SidepanelThemeName = (typeof SIDEPANEL_THEME_NAMES)[number];

interface SidepanelThemeTokens {
  base100: string;
  base200: string;
  base300: string;
  baseContent: string;
  primary: string;
  secondary: string;
  accent: string;
  neutral: string;
  success: string;
  warning: string;
  error: string;
}

export interface SidepanelThemePreset {
  name: SidepanelThemeName;
  labelKey: TranslationKey;
  tokens: SidepanelThemeTokens;
}

const STORAGE_KEY = "matrixhub-sidepanel-theme";
const DEFAULT_THEME: SidepanelThemeName = "business";

const LIGHT_THEMES = new Set<SidepanelThemeName>([
  "corporate",
  "light",
  "nord",
  "retro",
]);

const themePrimaryContentTokens: Record<SidepanelThemeName, string> = {
  business: "oklch(88.34% 0.019 251.473)",
  corporate: "oklch(100% 0 0)",
  night: "oklch(15.07% 0.027 232.661)",
  black: "oklch(100% 0 0)",
  luxury: "oklch(20% 0 0)",
  dracula: "oklch(15.092% 0.036 346.812)",
  forest: "oklch(0% 0 0)",
  coffee: "oklch(14.399% 0.024 62.756)",
  dim: "oklch(17.226% 0.028 139.549)",
  sunset: "oklch(14.94% 0.031 39.947)",
  halloween: "oklch(19.693% 0.004 196.779)",
  synthwave: "oklch(28% 0.109 3.907)",
  light: "oklch(93% 0.034 272.788)",
  nord: "oklch(11.887% 0.015 254.027)",
  retro: "oklch(39% 0.141 25.723)",
};

const themeTokens: Record<SidepanelThemeName, SidepanelThemeTokens> = {
  business: {
    base100: "oklch(24.353% 0 0)",
    base200: "oklch(22.648% 0 0)",
    base300: "oklch(20.944% 0 0)",
    baseContent: "oklch(84.87% 0 0)",
    primary: "oklch(41.703% 0.099 251.473)",
    secondary: "oklch(64.092% 0.027 229.389)",
    accent: "oklch(67.271% 0.167 35.791)",
    neutral: "oklch(27.441% 0.013 253.041)",
    success: "oklch(70.226% 0.094 156.596)",
    warning: "oklch(77.482% 0.115 81.519)",
    error: "oklch(51.61% 0.146 29.674)",
  },
  corporate: {
    base100: "oklch(100% 0 0)",
    base200: "oklch(93% 0 0)",
    base300: "oklch(86% 0 0)",
    baseContent: "oklch(22.389% 0.031 278.072)",
    primary: "oklch(58% 0.158 241.966)",
    secondary: "oklch(55% 0.046 257.417)",
    accent: "oklch(60% 0.118 184.704)",
    neutral: "oklch(0% 0 0)",
    success: "oklch(62% 0.194 149.214)",
    warning: "oklch(85% 0.199 91.936)",
    error: "oklch(70% 0.191 22.216)",
  },
  night: {
    base100: "oklch(20.768% 0.039 265.754)",
    base200: "oklch(19.314% 0.037 265.754)",
    base300: "oklch(17.86% 0.034 265.754)",
    baseContent: "oklch(84.153% 0.007 265.754)",
    primary: "oklch(75.351% 0.138 232.661)",
    secondary: "oklch(68.011% 0.158 276.934)",
    accent: "oklch(72.36% 0.176 350.048)",
    neutral: "oklch(27.949% 0.036 260.03)",
    success: "oklch(78.452% 0.132 181.911)",
    warning: "oklch(83.242% 0.139 82.95)",
    error: "oklch(71.785% 0.17 13.118)",
  },
  black: {
    base100: "oklch(0% 0 0)",
    base200: "oklch(19% 0 0)",
    base300: "oklch(22% 0 0)",
    baseContent: "oklch(87.609% 0 0)",
    primary: "oklch(35% 0 0)",
    secondary: "oklch(35% 0 0)",
    accent: "oklch(35% 0 0)",
    neutral: "oklch(35% 0 0)",
    success: "oklch(51.975% 0.176 142.495)",
    warning: "oklch(96.798% 0.211 109.769)",
    error: "oklch(62.795% 0.257 29.233)",
  },
  luxury: {
    base100: "oklch(14.076% 0.004 285.822)",
    base200: "oklch(20.219% 0.004 308.229)",
    base300: "oklch(23.219% 0.004 308.229)",
    baseContent: "oklch(75.687% 0.123 76.89)",
    primary: "oklch(100% 0 0)",
    secondary: "oklch(27.581% 0.064 261.069)",
    accent: "oklch(36.674% 0.051 338.825)",
    neutral: "oklch(24.27% 0.057 59.825)",
    success: "oklch(78.119% 0.192 132.154)",
    warning: "oklch(86.127% 0.136 102.891)",
    error: "oklch(71.753% 0.176 22.568)",
  },
  dracula: {
    base100: "oklch(28.822% 0.022 277.508)",
    base200: "oklch(26.805% 0.02 277.508)",
    base300: "oklch(24.787% 0.019 277.508)",
    baseContent: "oklch(97.747% 0.007 106.545)",
    primary: "oklch(75.461% 0.183 346.812)",
    secondary: "oklch(74.202% 0.148 301.883)",
    accent: "oklch(83.392% 0.124 66.558)",
    neutral: "oklch(39.445% 0.032 275.524)",
    success: "oklch(87.099% 0.219 148.024)",
    warning: "oklch(95.533% 0.134 112.757)",
    error: "oklch(68.22% 0.206 24.43)",
  },
  forest: {
    base100: "oklch(20.84% 0.008 17.911)",
    base200: "oklch(18.522% 0.007 17.911)",
    base300: "oklch(16.203% 0.007 17.911)",
    baseContent: "oklch(83.768% 0.001 17.911)",
    primary: "oklch(68.628% 0.185 148.958)",
    secondary: "oklch(69.776% 0.135 168.327)",
    accent: "oklch(70.628% 0.119 185.713)",
    neutral: "oklch(30.698% 0.039 171.364)",
    success: "oklch(64.8% 0.15 160)",
    warning: "oklch(84.71% 0.199 83.87)",
    error: "oklch(71.76% 0.221 22.18)",
  },
  coffee: {
    base100: "oklch(24% 0.023 329.708)",
    base200: "oklch(21% 0.021 329.708)",
    base300: "oklch(16% 0.019 329.708)",
    baseContent: "oklch(72.354% 0.092 79.129)",
    primary: "oklch(71.996% 0.123 62.756)",
    secondary: "oklch(34.465% 0.029 199.194)",
    accent: "oklch(42.621% 0.074 224.389)",
    neutral: "oklch(16.51% 0.015 326.261)",
    success: "oklch(74.722% 0.072 131.116)",
    warning: "oklch(88.15% 0.14 87.722)",
    error: "oklch(77.318% 0.128 31.871)",
  },
  dim: {
    base100: "oklch(30.857% 0.023 264.149)",
    base200: "oklch(28.036% 0.019 264.182)",
    base300: "oklch(26.346% 0.018 262.177)",
    baseContent: "oklch(82.901% 0.031 222.959)",
    primary: "oklch(86.133% 0.141 139.549)",
    secondary: "oklch(73.375% 0.165 35.353)",
    accent: "oklch(74.229% 0.133 311.379)",
    neutral: "oklch(24.731% 0.02 264.094)",
    success: "oklch(86.171% 0.142 166.534)",
    warning: "oklch(86.163% 0.142 94.818)",
    error: "oklch(82.418% 0.099 33.756)",
  },
  sunset: {
    base100: "oklch(22% 0.019 237.69)",
    base200: "oklch(20% 0.019 237.69)",
    base300: "oklch(18% 0.019 237.69)",
    baseContent: "oklch(77.383% 0.043 245.096)",
    primary: "oklch(74.703% 0.158 39.947)",
    secondary: "oklch(72.537% 0.177 2.72)",
    accent: "oklch(71.294% 0.166 299.844)",
    neutral: "oklch(26% 0.019 237.69)",
    success: "oklch(85.56% 0.085 144.778)",
    warning: "oklch(85.569% 0.084 74.427)",
    error: "oklch(85.511% 0.078 16.886)",
  },
  halloween: {
    base100: "oklch(21% 0.006 56.043)",
    base200: "oklch(14% 0.004 49.25)",
    base300: "oklch(0% 0 0)",
    baseContent: "oklch(84.955% 0 0)",
    primary: "oklch(77.48% 0.204 60.62)",
    secondary: "oklch(45.98% 0.248 305.03)",
    accent: "oklch(64.8% 0.223 136.073)",
    neutral: "oklch(24.371% 0.046 65.681)",
    success: "oklch(62.705% 0.169 149.213)",
    warning: "oklch(66.584% 0.157 58.318)",
    error: "oklch(65.72% 0.199 27.33)",
  },
  synthwave: {
    base100: "oklch(15% 0.09 281.288)",
    base200: "oklch(20% 0.09 281.288)",
    base300: "oklch(25% 0.09 281.288)",
    baseContent: "oklch(78% 0.115 274.713)",
    primary: "oklch(71% 0.202 349.761)",
    secondary: "oklch(82% 0.111 230.318)",
    accent: "oklch(75% 0.183 55.934)",
    neutral: "oklch(45% 0.24 277.023)",
    success: "oklch(77% 0.152 181.912)",
    warning: "oklch(90% 0.182 98.111)",
    error: "oklch(73.7% 0.121 32.639)",
  },
  light: {
    base100: "oklch(100% 0 0)",
    base200: "oklch(98% 0 0)",
    base300: "oklch(95% 0 0)",
    baseContent: "oklch(21% 0.006 285.885)",
    primary: "oklch(45% 0.24 277.023)",
    secondary: "oklch(65% 0.241 354.308)",
    accent: "oklch(77% 0.152 181.912)",
    neutral: "oklch(14% 0.005 285.823)",
    success: "oklch(76% 0.177 163.223)",
    warning: "oklch(82% 0.189 84.429)",
    error: "oklch(71% 0.194 13.428)",
  },
  nord: {
    base100: "oklch(95.127% 0.007 260.731)",
    base200: "oklch(93.299% 0.01 261.788)",
    base300: "oklch(89.925% 0.016 262.749)",
    baseContent: "oklch(32.437% 0.022 264.182)",
    primary: "oklch(59.435% 0.077 254.027)",
    secondary: "oklch(69.651% 0.059 248.687)",
    accent: "oklch(77.464% 0.062 217.469)",
    neutral: "oklch(45.229% 0.035 264.131)",
    success: "oklch(76.827% 0.074 131.063)",
    warning: "oklch(85.486% 0.089 84.093)",
    error: "oklch(60.61% 0.12 15.341)",
  },
  retro: {
    base100: "oklch(91.637% 0.034 90.515)",
    base200: "oklch(88.272% 0.049 91.774)",
    base300: "oklch(84.133% 0.065 90.856)",
    baseContent: "oklch(41% 0.112 45.904)",
    primary: "oklch(80% 0.114 19.571)",
    secondary: "oklch(92% 0.084 155.995)",
    accent: "oklch(68% 0.162 75.834)",
    neutral: "oklch(44% 0.011 73.639)",
    success: "oklch(51% 0.096 186.391)",
    warning: "oklch(64% 0.222 41.116)",
    error: "oklch(70% 0.191 22.216)",
  },
};

const themeLabelKeys: Record<SidepanelThemeName, TranslationKey> = {
  business: "theme.names.business",
  corporate: "theme.names.corporate",
  night: "theme.names.night",
  black: "theme.names.black",
  luxury: "theme.names.luxury",
  dracula: "theme.names.dracula",
  forest: "theme.names.forest",
  coffee: "theme.names.coffee",
  dim: "theme.names.dim",
  sunset: "theme.names.sunset",
  halloween: "theme.names.halloween",
  synthwave: "theme.names.synthwave",
  light: "theme.names.light",
  nord: "theme.names.nord",
  retro: "theme.names.retro",
};

export const SIDEPANEL_THEMES: SidepanelThemePreset[] =
  SIDEPANEL_THEME_NAMES.map((name) => ({
    name,
    labelKey: themeLabelKeys[name],
    tokens: themeTokens[name],
  }));

const themeByName = new Map(
  SIDEPANEL_THEMES.map((theme) => [theme.name, theme]),
);

export function getSidepanelTheme(name?: string | null): SidepanelThemePreset {
  return (
    themeByName.get(normalizeSidepanelThemeName(name)) ??
    themeByName.get(DEFAULT_THEME)!
  );
}

export function normalizeSidepanelThemeName(
  name?: string | null,
): SidepanelThemeName {
  if (name && themeByName.has(name as SidepanelThemeName)) {
    return name as SidepanelThemeName;
  }

  return DEFAULT_THEME;
}

export function readStoredSidepanelTheme(): SidepanelThemeName {
  if (typeof localStorage === "undefined") {
    return DEFAULT_THEME;
  }

  try {
    return normalizeSidepanelThemeName(localStorage.getItem(STORAGE_KEY));
  } catch {
    return DEFAULT_THEME;
  }
}

export function applySidepanelTheme(
  name: SidepanelThemeName,
  root: HTMLElement = document.documentElement,
) {
  const theme = getSidepanelTheme(name);

  root.dataset.sidepanelTheme = theme.name;
  root.style.colorScheme = LIGHT_THEMES.has(theme.name) ? "light" : "dark";
  root.style.setProperty("--theme-base-100", theme.tokens.base100);
  root.style.setProperty("--theme-base-200", theme.tokens.base200);
  root.style.setProperty("--theme-base-300", theme.tokens.base300);
  root.style.setProperty("--theme-base-content", theme.tokens.baseContent);
  root.style.setProperty("--theme-primary", theme.tokens.primary);
  root.style.setProperty(
    "--theme-primary-content",
    themePrimaryContentTokens[theme.name],
  );
  root.style.setProperty("--theme-secondary", theme.tokens.secondary);
  root.style.setProperty("--theme-accent", theme.tokens.accent);
  root.style.setProperty("--theme-neutral", theme.tokens.neutral);
  root.style.setProperty("--theme-success", theme.tokens.success);
  root.style.setProperty("--theme-warning", theme.tokens.warning);
  root.style.setProperty("--theme-error", theme.tokens.error);

  return theme;
}

export function persistSidepanelTheme(name: SidepanelThemeName) {
  if (typeof localStorage === "undefined") {
    return;
  }

  try {
    localStorage.setItem(STORAGE_KEY, name);
  } catch {
    // Ignore storage write failures in extension pages.
  }
}

export function setSidepanelTheme(
  name: SidepanelThemeName,
  root: HTMLElement = document.documentElement,
) {
  persistSidepanelTheme(name);
  return applySidepanelTheme(name, root);
}

export function applyStoredSidepanelTheme(
  root: HTMLElement = document.documentElement,
) {
  return applySidepanelTheme(readStoredSidepanelTheme(), root);
}
