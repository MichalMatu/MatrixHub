<script lang="ts">
  import { fly } from "svelte/transition";
  import Check from "~icons/tabler/check";
  import Palette from "~icons/tabler/palette";
  import { i18n } from "$lib/i18n/store";
  import {
    SIDEPANEL_THEMES,
    type SidepanelThemeName
  } from "$lib/features/theme/themePresets";

  export let activeTheme: SidepanelThemeName;
  export let isOpen = false;
  export let mode: "dock" | "inline" = "dock";
  export let onToggle: () => void | Promise<void> = () => undefined;
  export let onSelectTheme: (theme: SidepanelThemeName) => void | Promise<void> = () => undefined;

  function getThemeLabel(theme: (typeof SIDEPANEL_THEMES)[number]) {
    return $i18n.t(theme.labelKey);
  }

  $: activeThemeOption =
    SIDEPANEL_THEMES.find((theme) => theme.name === activeTheme) ?? null;
</script>

{#if mode === "dock"}
  <div class="theme-dock">
    {#if isOpen}
      <section class="theme-palette panel" aria-label={$i18n.t("theme.paletteLabel")}>
        <div class="theme-palette-head">
          <strong>{$i18n.t("theme.styles")}</strong>
          <span class="theme-active-label">
            {#if activeThemeOption}
              {getThemeLabel(activeThemeOption)}
            {/if}
          </span>
        </div>

        <div class="theme-grid" role="list">
          {#each SIDEPANEL_THEMES as theme (theme.name)}
            <button
              aria-label={$i18n.t("theme.useStyle", {
                style: getThemeLabel(theme),
              })}
              type="button"
              class:selected={theme.name === activeTheme}
              class="theme-option"
              title={getThemeLabel(theme)}
              on:click={() => onSelectTheme(theme.name)}
            >
              <span
                class="theme-preview"
                style={`--preview-base:${theme.tokens.base100};--preview-content:${theme.tokens.baseContent};--preview-primary:${theme.tokens.primary};--preview-secondary:${theme.tokens.secondary};--preview-accent:${theme.tokens.accent};`}
              >
                <span class="theme-preview-strip preview-primary"></span>
                <span class="theme-preview-strip preview-secondary"></span>
                <span class="theme-preview-strip preview-accent"></span>
              </span>

              {#if theme.name === activeTheme}
                <Check aria-hidden="true" class="theme-check" />
              {/if}
            </button>
          {/each}
        </div>
      </section>
    {/if}

    <button
      aria-expanded={isOpen}
      aria-label={$i18n.t("theme.openPalette")}
      class:open={isOpen}
      class="theme-fab"
      title={$i18n.t("theme.styles")}
      type="button"
      on:click={onToggle}
    >
      <Palette aria-hidden="true" class="theme-fab-icon" />
    </button>
  </div>
{:else if isOpen}
  <section
    aria-label={$i18n.t("theme.paletteLabel")}
    class="theme-palette theme-palette-inline panel"
    in:fly={{ y: -8, duration: 180 }}
    out:fly={{ y: -8, duration: 140 }}
  >
    <div class="theme-palette-head">
      <strong>{$i18n.t("theme.styles")}</strong>
      <span class="theme-active-label">
        {#if activeThemeOption}
          {getThemeLabel(activeThemeOption)}
        {/if}
      </span>
    </div>

    <div class="theme-grid" role="list">
      {#each SIDEPANEL_THEMES as theme (theme.name)}
        <button
          aria-label={$i18n.t("theme.useStyle", {
            style: getThemeLabel(theme),
          })}
          type="button"
          class:selected={theme.name === activeTheme}
          class="theme-option"
          title={getThemeLabel(theme)}
          on:click={() => onSelectTheme(theme.name)}
        >
          <span
            class="theme-preview"
            style={`--preview-base:${theme.tokens.base100};--preview-content:${theme.tokens.baseContent};--preview-primary:${theme.tokens.primary};--preview-secondary:${theme.tokens.secondary};--preview-accent:${theme.tokens.accent};`}
          >
            <span class="theme-preview-strip preview-primary"></span>
            <span class="theme-preview-strip preview-secondary"></span>
            <span class="theme-preview-strip preview-accent"></span>
          </span>

          {#if theme.name === activeTheme}
            <Check aria-hidden="true" class="theme-check" />
          {/if}
        </button>
      {/each}
    </div>
  </section>
{/if}
