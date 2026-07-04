<script lang="ts">
  import type { MatrixSettings } from "@matrixhub/device-sdk";
  import Button from "$lib/components/ui/Button.svelte";
  import {
    buildMatrixSettingsPatch,
    createMatrixSettingsDraft,
    formatMatrixAlarmMode,
    formatMatrixEffectState,
    formatMatrixMenuState,
  } from "$lib/features/matrix/model";
  import { i18n } from "$lib/i18n/store";

  export let settings: MatrixSettings | null = null;
  export let loading = false;
  export let saving = false;
  export let canManage = false;
  export let showingCachedData = false;
  export let errorMessage: string | null = null;
  export let onReload: () => void | Promise<void> = () => undefined;
  export let onSave: (
    settings: Partial<MatrixSettings>,
  ) => void | Promise<void> = () => undefined;

  let lastSettings: MatrixSettings | null = settings;
  let draft = createMatrixSettingsDraft(settings);

  $: if (settings !== lastSettings) {
    lastSettings = settings;
    draft = createMatrixSettingsDraft(settings);
  }

  function handleSubmit() {
    void onSave(buildMatrixSettingsPatch(draft));
  }
</script>

<section class="panel settings-shell">
  <div class="section-head compact-head">
    <h2>{$i18n.t("matrix.panel.title")}</h2>
  </div>

  {#if showingCachedData}
    <p class="panel-copy cache-note">
      {$i18n.t("common.cachedUntilLiveRefresh")}
    </p>
  {/if}

  {#if errorMessage}
    <p class="field-help error">
      {settings
        ? $i18n.t("matrix.panel.liveRefreshFailed", {
            message: errorMessage,
          })
        : errorMessage}
    </p>
  {/if}

  {#if loading && !settings}
    <p class="panel-copy">{$i18n.t("matrix.panel.loading")}</p>
  {:else if settings}
    <div class="settings-chip-row" aria-label={$i18n.t("matrix.panel.currentState")}>
      <span class="settings-chip">
        {$i18n.t("matrix.panel.mode")}:
        {formatMatrixAlarmMode(settings.alarm_mode, $i18n)}
      </span>
      <span
        class="settings-chip"
        data-state={settings.effect_enabled ? "on" : "off"}
      >
        {$i18n.t("matrix.panel.effects")}:
        {formatMatrixEffectState(settings.effect_enabled, $i18n)}
      </span>
      <span
        class="settings-chip"
        data-state={settings.menu_enabled ? "on" : "off"}
      >
        {$i18n.t("matrix.panel.menu")}:
        {formatMatrixMenuState(settings.menu_enabled, $i18n)}
      </span>
    </div>

    <form class="stack" on:submit|preventDefault={handleSubmit}>
      <div class="settings-two-up">
        <label class="field">
          <span>{$i18n.t("matrix.panel.brightness")}</span>
          <input
            bind:value={draft.brightness}
            type="number"
            min="0"
            max="255"
            step="1"
            disabled={!canManage || saving}
          />
        </label>

        <label class="field">
          <span>{$i18n.t("matrix.panel.scrollSpeed")}</span>
          <input
            bind:value={draft.menuScrollSpeed}
            type="number"
            min="20"
            max="120"
            step="1"
            disabled={!canManage || saving}
          />
        </label>
      </div>

      <label class="matrix-toggle-row">
        <div class="matrix-toggle-copy">
          <span>{$i18n.t("matrix.panel.ledEffects")}</span>
          <small>{$i18n.t("matrix.panel.animatedBackground")}</small>
        </div>

        <span class="matrix-toggle-control">
          <input
            class="matrix-toggle-input"
            type="checkbox"
            bind:checked={draft.effectEnabled}
            disabled={!canManage || saving}
          />
          <span class="matrix-toggle-track" aria-hidden="true">
            <span class="matrix-toggle-thumb"></span>
          </span>
        </span>
      </label>

      {#if !canManage}
        <p class="panel-copy">{$i18n.t("matrix.panel.adminRequired")}</p>
      {/if}

      <div class="button-row">
        <Button
          class="matrix-submit"
          variant="primary"
          type="submit"
          disabled={!canManage || saving}
        >
          {saving ? $i18n.t("forms.addDevice.saving") : $i18n.t("common.save")}
        </Button>
        <Button
          variant="ghost"
          disabled={loading || saving}
          onClick={onReload}
        >
          {$i18n.t("common.refresh")}
        </Button>
      </div>
    </form>
  {:else}
    <p class="panel-copy">
      {#if errorMessage}
        {$i18n.t("matrix.panel.retryRefresh")}
      {:else}
        {$i18n.t("matrix.panel.appearAfterRestore")}
      {/if}
    </p>

    <div class="button-row">
      <Button variant="ghost" disabled={loading || saving} onClick={onReload}>
        {$i18n.t("common.retry")}
      </Button>
    </div>
  {/if}
</section>
