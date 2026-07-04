import type { MatrixSettings } from "@matrixhub/device-sdk";
import type { I18nRuntime } from "$lib/i18n/runtime";

export interface MatrixSettingsDraft {
  brightness: number;
  effectEnabled: boolean;
  menuScrollSpeed: number;
}

export const DEFAULT_MATRIX_SETTINGS: MatrixSettings = {
  brightness: 20,
  alarm_mode: 1,
  rotation: 0,
  auto_rotate: false,
  effect_enabled: false,
  effect_engine: 0,
  effect_mode: 0,
  effect_speed: 1000,
  effect_color: 0x00ff00,
  effect_color_2: 0xff0000,
  effect_color_3: 0x0000ff,
  effect_reactivity_provider: 0,
  effect_reactivity_gain: 80,
  background_mode: 0,
  data_visualization_enabled: false,
  data_visualization_source: 0,
  data_visualization_metric: 0,
  data_visualization_mode: 0,
  data_visualization_min: 400,
  data_visualization_max: 2000,
  data_visualization_color_min: 0x00ff80,
  data_visualization_color_mid: 0xffd166,
  data_visualization_color_max: 0xff3000,
  data_visualization_brightness_min: 12,
  data_visualization_brightness_max: 180,
  data_visualization_smoothing: 50,
  data_visualization_stale_behavior: 0,
  data_visualization_device_id: "",
  menu_enabled: true,
  menu_text_color: 0xffffff,
  menu_scroll_speed: 20,
};

export const MATRIX_ALARM_MODE_OPTIONS = [
  {
    value: 0,
    key: "solid",
  },
  {
    value: 1,
    key: "icon",
  },
  {
    value: 2,
    key: "scroll",
  },
] as const;

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }

  return Math.min(max, Math.max(min, Math.round(value)));
}

export function createMatrixSettingsDraft(
  settings: MatrixSettings | null,
): MatrixSettingsDraft {
  const base = settings ?? DEFAULT_MATRIX_SETTINGS;

  return {
    brightness: clampInteger(base.brightness, 0, 255),
    effectEnabled: base.effect_enabled,
    menuScrollSpeed: clampInteger(base.menu_scroll_speed, 20, 120),
  };
}

export function buildMatrixSettingsPatch(
  draft: MatrixSettingsDraft,
): Partial<MatrixSettings> {
  return {
    brightness: clampInteger(draft.brightness, 0, 255),
    effect_enabled: draft.effectEnabled,
    menu_scroll_speed: clampInteger(draft.menuScrollSpeed, 20, 120),
  };
}

export function formatMatrixMenuState(enabled: boolean, i18n: I18nRuntime) {
  return i18n.t(enabled ? "matrix.state.enabled" : "matrix.state.disabled");
}

export function formatMatrixEffectState(enabled: boolean, i18n: I18nRuntime) {
  return i18n.t(enabled ? "matrix.state.enabled" : "matrix.state.disabled");
}

export function describeMatrixAlarmMode(value: number) {
  return (
    MATRIX_ALARM_MODE_OPTIONS.find((option) => option.value === value)?.key ??
    null
  );
}

export function formatMatrixAlarmMode(value: number, i18n: I18nRuntime) {
  const key = describeMatrixAlarmMode(value);
  return key ? i18n.t(`matrix.mode.${key}`) : i18n.t("matrix.mode.unknown");
}
