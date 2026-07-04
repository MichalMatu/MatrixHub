import {
  createDeviceApiClient,
  type DeviceApiClientOptions,
} from "./api/client";

export interface MatrixSettings {
  brightness: number;
  alarm_mode: number;
  rotation: number;
  auto_rotate: boolean;
  effect_enabled: boolean;
  effect_engine: number;
  effect_mode: number;
  effect_speed: number;
  effect_color: number;
  effect_color_2: number;
  effect_color_3: number;
  effect_reactivity_provider: number;
  effect_reactivity_gain: number;
  background_mode: number;
  data_visualization_enabled: boolean;
  data_visualization_source: number;
  data_visualization_metric: number;
  data_visualization_mode: number;
  data_visualization_min: number;
  data_visualization_max: number;
  data_visualization_color_min: number;
  data_visualization_color_mid: number;
  data_visualization_color_max: number;
  data_visualization_brightness_min: number;
  data_visualization_brightness_max: number;
  data_visualization_smoothing: number;
  data_visualization_stale_behavior: number;
  data_visualization_device_id: string;
  custom_icons?: number[][];
  menu_enabled: boolean;
  menu_text_color: number;
  menu_scroll_speed: number;
}

const MATRIX_COLOR_MASK = 0xffffff;
const MATRIX_CUSTOM_ICON_SLOTS = 3;
const MATRIX_CUSTOM_ICON_PIXELS = 64;
const MATRIX_EFFECT_ENGINE_MAX = 1;
const MATRIX_EFFECT_MODE_MAX = 69;
const MATRIX_NATIVE_3D_EFFECT_MODE_MAX = 3;
const MATRIX_EFFECT_REACTIVITY_PROVIDER_MAX = 1;
const MATRIX_EFFECT_REACTIVITY_GAIN_MAX = 200;
const MATRIX_EFFECT_SPEED_MIN = 50;
const MATRIX_EFFECT_SPEED_MAX = 24 * 60 * 60 * 1000;
const MATRIX_BACKGROUND_MODE_MAX = 1;
const MATRIX_DATA_VISUALIZATION_SOURCE_MAX = 3;
const MATRIX_DATA_VISUALIZATION_METRIC_MAX = 5;
const MATRIX_DATA_VISUALIZATION_MODE_MAX = 6;
const MATRIX_DATA_VISUALIZATION_STALE_BEHAVIOR_MAX = 2;
const MATRIX_DATA_VISUALIZATION_DEVICE_ID_MAX = 17;

const DEFAULT_DATA_VISUALIZATION = {
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
} satisfies Pick<
  MatrixSettings,
  | "background_mode"
  | "data_visualization_enabled"
  | "data_visualization_source"
  | "data_visualization_metric"
  | "data_visualization_mode"
  | "data_visualization_min"
  | "data_visualization_max"
  | "data_visualization_color_min"
  | "data_visualization_color_mid"
  | "data_visualization_color_max"
  | "data_visualization_brightness_min"
  | "data_visualization_brightness_max"
  | "data_visualization_smoothing"
  | "data_visualization_stale_behavior"
  | "data_visualization_device_id"
>;

function isObject(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object";
}

function optionalNumber(value: unknown) {
  return typeof value === "number" && Number.isFinite(value)
    ? value
    : undefined;
}

function optionalBoolean(value: unknown) {
  return typeof value === "boolean" ? value : undefined;
}

function optionalString(value: unknown) {
  return typeof value === "string" ? value : undefined;
}

function clampInteger(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, Math.round(value)));
}

function normalizeColor(value: number) {
  return Math.max(0, Math.round(value)) & MATRIX_COLOR_MASK;
}

function sanitizeCustomIcons(value: unknown): number[][] | null | undefined {
  if (!Array.isArray(value)) {
    return undefined;
  }

  if (value.length !== MATRIX_CUSTOM_ICON_SLOTS) {
    return null;
  }

  const icons: number[][] = [];
  for (const entry of value) {
    if (!Array.isArray(entry)) {
      return null;
    }

    if (entry.length === 0) {
      icons.push([]);
      continue;
    }

    if (entry.length !== MATRIX_CUSTOM_ICON_PIXELS) {
      return null;
    }

    const pixels: number[] = [];
    for (const pixel of entry) {
      if (typeof pixel !== "number" || !Number.isFinite(pixel)) {
        return null;
      }
      pixels.push(normalizeColor(pixel));
    }
    icons.push(pixels);
  }

  return icons;
}

export function parseMatrixSettings(value: unknown): MatrixSettings | null {
  if (!isObject(value)) {
    return null;
  }

  const brightness = optionalNumber(value.brightness);
  const alarmMode = optionalNumber(value.alarm_mode);
  const rotation = optionalNumber(value.rotation);
  const autoRotate = optionalBoolean(value.auto_rotate);
  const effectEnabled = optionalBoolean(value.effect_enabled);
  const effectEngine = optionalNumber(value.effect_engine);
  const effectMode = optionalNumber(value.effect_mode);
  const effectSpeed = optionalNumber(value.effect_speed);
  const effectColor = optionalNumber(value.effect_color);
  const effectColor2 = optionalNumber(value.effect_color_2);
  const effectColor3 = optionalNumber(value.effect_color_3);
  const effectReactivityProvider = optionalNumber(value.effect_reactivity_provider);
  const effectReactivityGain = optionalNumber(value.effect_reactivity_gain);
  const backgroundMode =
    optionalNumber(value.background_mode) ??
    DEFAULT_DATA_VISUALIZATION.background_mode;
  const dataVisualizationEnabled =
    optionalBoolean(value.data_visualization_enabled) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_enabled;
  const dataVisualizationSource =
    optionalNumber(value.data_visualization_source) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_source;
  const dataVisualizationMetric =
    optionalNumber(value.data_visualization_metric) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_metric;
  const dataVisualizationMode =
    optionalNumber(value.data_visualization_mode) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_mode;
  const dataVisualizationMin =
    optionalNumber(value.data_visualization_min) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_min;
  const dataVisualizationMax =
    optionalNumber(value.data_visualization_max) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_max;
  const dataVisualizationColorMin =
    optionalNumber(value.data_visualization_color_min) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_color_min;
  const dataVisualizationColorMid =
    optionalNumber(value.data_visualization_color_mid) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_color_mid;
  const dataVisualizationColorMax =
    optionalNumber(value.data_visualization_color_max) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_color_max;
  const dataVisualizationBrightnessMin =
    optionalNumber(value.data_visualization_brightness_min) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_brightness_min;
  const dataVisualizationBrightnessMax =
    optionalNumber(value.data_visualization_brightness_max) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_brightness_max;
  const dataVisualizationSmoothing =
    optionalNumber(value.data_visualization_smoothing) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_smoothing;
  const dataVisualizationStaleBehavior =
    optionalNumber(value.data_visualization_stale_behavior) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_stale_behavior;
  const dataVisualizationDeviceId =
    optionalString(value.data_visualization_device_id) ??
    DEFAULT_DATA_VISUALIZATION.data_visualization_device_id;
  const menuEnabled = optionalBoolean(value.menu_enabled);
  const menuTextColor = optionalNumber(value.menu_text_color);
  const menuScrollSpeed = optionalNumber(value.menu_scroll_speed);

  if (
    brightness === undefined ||
    alarmMode === undefined ||
    rotation === undefined ||
    autoRotate === undefined ||
    effectEnabled === undefined ||
    effectEngine === undefined ||
    effectMode === undefined ||
    effectSpeed === undefined ||
    effectColor === undefined ||
    effectColor2 === undefined ||
    effectColor3 === undefined ||
    effectReactivityProvider === undefined ||
    effectReactivityGain === undefined ||
    menuEnabled === undefined ||
    menuTextColor === undefined ||
    menuScrollSpeed === undefined
  ) {
    return null;
  }

  const normalizedEffectEngine = clampInteger(
    effectEngine,
    0,
    MATRIX_EFFECT_ENGINE_MAX,
  );
  const effectModeMax =
    normalizedEffectEngine === 1
      ? MATRIX_NATIVE_3D_EFFECT_MODE_MAX
      : MATRIX_EFFECT_MODE_MAX;
  const normalizedDataVisualizationMin = Number.isFinite(dataVisualizationMin)
    ? dataVisualizationMin
    : DEFAULT_DATA_VISUALIZATION.data_visualization_min;
  const normalizedDataVisualizationMax =
    Number.isFinite(dataVisualizationMax) &&
    dataVisualizationMax > normalizedDataVisualizationMin
      ? dataVisualizationMax
      : normalizedDataVisualizationMin + 1;
  const normalizedBrightnessMin = clampInteger(
    dataVisualizationBrightnessMin,
    0,
    255,
  );
  const normalizedBrightnessMax = clampInteger(
    dataVisualizationBrightnessMax,
    0,
    255,
  );

  const settings: MatrixSettings = {
    brightness: clampInteger(brightness, 0, 255),
    alarm_mode: clampInteger(alarmMode, 0, 2),
    rotation: clampInteger(rotation, 0, 3),
    auto_rotate: autoRotate,
    effect_enabled: effectEnabled,
    effect_engine: normalizedEffectEngine,
    effect_mode: clampInteger(effectMode, 0, effectModeMax),
    effect_speed: clampInteger(
      effectSpeed,
      MATRIX_EFFECT_SPEED_MIN,
      MATRIX_EFFECT_SPEED_MAX,
    ),
    effect_color: normalizeColor(effectColor),
    effect_color_2: normalizeColor(effectColor2),
    effect_color_3: normalizeColor(effectColor3),
    effect_reactivity_provider: clampInteger(
      effectReactivityProvider,
      0,
      MATRIX_EFFECT_REACTIVITY_PROVIDER_MAX,
    ),
    effect_reactivity_gain: clampInteger(
      effectReactivityGain,
      0,
      MATRIX_EFFECT_REACTIVITY_GAIN_MAX,
    ),
    background_mode: clampInteger(
      backgroundMode,
      0,
      MATRIX_BACKGROUND_MODE_MAX,
    ),
    data_visualization_enabled: dataVisualizationEnabled,
    data_visualization_source: clampInteger(
      dataVisualizationSource,
      0,
      MATRIX_DATA_VISUALIZATION_SOURCE_MAX,
    ),
    data_visualization_metric: clampInteger(
      dataVisualizationMetric,
      0,
      MATRIX_DATA_VISUALIZATION_METRIC_MAX,
    ),
    data_visualization_mode: clampInteger(
      dataVisualizationMode,
      0,
      MATRIX_DATA_VISUALIZATION_MODE_MAX,
    ),
    data_visualization_min: normalizedDataVisualizationMin,
    data_visualization_max: normalizedDataVisualizationMax,
    data_visualization_color_min: normalizeColor(dataVisualizationColorMin),
    data_visualization_color_mid: normalizeColor(dataVisualizationColorMid),
    data_visualization_color_max: normalizeColor(dataVisualizationColorMax),
    data_visualization_brightness_min: Math.min(
      normalizedBrightnessMin,
      normalizedBrightnessMax,
    ),
    data_visualization_brightness_max: Math.max(
      normalizedBrightnessMin,
      normalizedBrightnessMax,
    ),
    data_visualization_smoothing: clampInteger(
      dataVisualizationSmoothing,
      0,
      100,
    ),
    data_visualization_stale_behavior: clampInteger(
      dataVisualizationStaleBehavior,
      0,
      MATRIX_DATA_VISUALIZATION_STALE_BEHAVIOR_MAX,
    ),
    data_visualization_device_id: dataVisualizationDeviceId.slice(
      0,
      MATRIX_DATA_VISUALIZATION_DEVICE_ID_MAX,
    ),
    menu_enabled: menuEnabled,
    menu_text_color: normalizeColor(menuTextColor),
    menu_scroll_speed: clampInteger(menuScrollSpeed, 20, 120),
  };

  const customIcons = sanitizeCustomIcons(value.custom_icons);
  if (customIcons === null) {
    return null;
  }
  if (customIcons) {
    settings.custom_icons = customIcons;
  }

  return settings;
}

function requireParsed<T>(value: T | null, message: string) {
  if (!value) {
    throw new Error(message);
  }

  return value;
}

export class DeviceMatrixApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSettings(): Promise<MatrixSettings> {
    const response = await this.client.get<unknown>("/api/matrix/settings");
    return requireParsed(
      parseMatrixSettings(response),
      "matrix/settings_invalid",
    );
  }

  async updateSettings(
    settings: Partial<MatrixSettings>,
  ): Promise<MatrixSettings> {
    const response = await this.client.post<unknown>(
      "/api/matrix/settings",
      settings,
    );
    return requireParsed(
      parseMatrixSettings(response),
      "matrix/settings_invalid",
    );
  }

  async calibrateCsiDataVisualization(): Promise<{ ok: boolean; status?: string }> {
    const response = await this.client.post<unknown>(
      "/api/matrix/data-visualization/csi/calibrate",
      {},
    );
    return isObject(response) && typeof response.ok === "boolean"
      ? { ok: response.ok, status: optionalString(response.status) }
      : { ok: false };
  }
}
