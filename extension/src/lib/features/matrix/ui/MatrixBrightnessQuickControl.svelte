<script lang="ts">
  import type { MatrixSettings } from "@matrixhub/device-sdk";
  import { i18n } from "$lib/i18n/store";

  export let settings: MatrixSettings;
  export let saving = false;
  export let onSave: (
    settings: Partial<MatrixSettings>,
    options?: { notify?: boolean },
  ) => void | Promise<void> = () => undefined;

  const MAX_BRIGHTNESS = 32;
  const MIN_VISIBLE_BRIGHTNESS = 3;
  const HIDDEN_BRIGHTNESS_STEPS = MIN_VISIBLE_BRIGHTNESS - 1;
  const SLIDER_MAX = MAX_BRIGHTNESS - HIDDEN_BRIGHTNESS_STEPS;

  let draftBrightness = normalizeBrightness(settings.brightness);
  let lastSettingsBrightness = settings.brightness;
  let lastSaving = saving;

  $: {
    const settingsChanged = settings.brightness !== lastSettingsBrightness;
    const saveFinished = lastSaving && !saving;

    if (settingsChanged || saveFinished) {
      draftBrightness = normalizeBrightness(settings.brightness);
      lastSettingsBrightness = settings.brightness;
    }

    lastSaving = saving;
  }

  $: sliderValue = brightnessToSliderValue(draftBrightness);
  $: sliderProgress = `${(sliderValue / SLIDER_MAX) * 100}%`;

  function clampBrightness(value: number) {
    if (!Number.isFinite(value)) {
      return 0;
    }

    return Math.min(MAX_BRIGHTNESS, Math.max(0, Math.round(value)));
  }

  function normalizeBrightness(value: number) {
    const clamped = clampBrightness(value);
    return clamped === 0 ? 0 : Math.max(MIN_VISIBLE_BRIGHTNESS, clamped);
  }

  function brightnessToSliderValue(value: number) {
    const normalized = normalizeBrightness(value);
    return normalized === 0 ? 0 : normalized - HIDDEN_BRIGHTNESS_STEPS;
  }

  function sliderToBrightness(value: number) {
    const clamped = Math.min(SLIDER_MAX, Math.max(0, Math.round(value)));
    return clamped === 0 ? 0 : clamped + HIDDEN_BRIGHTNESS_STEPS;
  }

  function handleInput(event: Event) {
    const target = event.currentTarget;
    if (!(target instanceof HTMLInputElement)) {
      return;
    }

    draftBrightness = sliderToBrightness(Number(target.value));
  }

  async function handleChange() {
    const nextBrightness = normalizeBrightness(draftBrightness);
    draftBrightness = nextBrightness;

    if (saving || nextBrightness === settings.brightness) {
      return;
    }

    await onSave(
      {
        brightness: nextBrightness,
      },
      { notify: false },
    );
  }
</script>

<div class="matrix-quick-control">
  <div class="matrix-quickbar" style={`--matrix-slider-progress:${sliderProgress};`}>
    <label class="matrix-range-shell">
      <span class="sr-only">{$i18n.t("matrix.quickControl.brightnessLabel")}</span>
      <input
        aria-label={$i18n.t("matrix.quickControl.brightnessLabel")}
        class="matrix-range"
        type="range"
        min="0"
        max={SLIDER_MAX}
        step="1"
        value={sliderValue}
        disabled={saving}
        on:input={handleInput}
        on:change={handleChange}
      />
    </label>
  </div>
</div>
