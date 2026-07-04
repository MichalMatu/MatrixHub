<script lang="ts">
  import { fly } from "svelte/transition";
  import ArrowLeft from "~icons/tabler/arrow-left";
  import PaletteIcon from "~icons/tabler/palette";
  import Plus from "~icons/tabler/plus";
  import Settings from "~icons/tabler/settings";
  import IconButton from "$lib/components/ui/IconButton.svelte";
  import OptionButton from "$lib/components/ui/OptionButton.svelte";
  import ThemePalette from "$lib/features/theme/ui/ThemePalette.svelte";
  import type { SidepanelThemeName } from "$lib/features/theme/themePresets";
  import { i18n } from "$lib/i18n/store";
  import type { LocalePreference } from "$lib/i18n/locales";
  import type { SidepanelState } from "../state/sidepanelState";

  export let activeScreen: SidepanelState["activeScreen"] = "devices";
  export let selectedDeviceName: string | null = null;
  export let activeTheme: SidepanelThemeName;
  export let localePreference: LocalePreference;
  export let onBack: () => void | Promise<void> = () => undefined;
  export let onOpenAddSheet: () => void | Promise<void> = () => undefined;
  export let onSelectTheme: (
    theme: SidepanelThemeName,
  ) => void | Promise<void> = () => undefined;
  export let onSelectLocale: (
    preference: LocalePreference,
  ) => void | Promise<void> = () => undefined;

  let isActionMenuOpen = false;
  let isThemePaletteOpen = false;
  let isLocaleListOpen = false;
  const localeOptions = [
    {
      value: "auto" as const,
      shortLabel: "A",
    },
    {
      value: "en" as const,
      shortLabel: "EN",
    },
    {
      value: "pl" as const,
      shortLabel: "PL",
    },
  ];

  function closeActionMenu() {
    isActionMenuOpen = false;
    isThemePaletteOpen = false;
    isLocaleListOpen = false;
  }

  function toggleHeaderActionMenu() {
    isActionMenuOpen = !isActionMenuOpen;
    if (!isActionMenuOpen) {
      isThemePaletteOpen = false;
      isLocaleListOpen = false;
    }
  }

  function toggleThemePalette() {
    if (!isActionMenuOpen) {
      isActionMenuOpen = true;
    }

    isLocaleListOpen = false;
    isThemePaletteOpen = !isThemePaletteOpen;
  }

  function toggleLocaleList() {
    if (!isActionMenuOpen) {
      isActionMenuOpen = true;
    }

    isThemePaletteOpen = false;
    isLocaleListOpen = !isLocaleListOpen;
  }

  async function handleOpenAddSheet() {
    closeActionMenu();
    await onOpenAddSheet();
  }

  async function handleSelectTheme(theme: SidepanelThemeName) {
    await onSelectTheme(theme);
    closeActionMenu();
  }

  async function handleSelectLocale(preference: LocalePreference) {
    await onSelectLocale(preference);
    closeActionMenu();
  }

  $: if (activeScreen !== "devices") {
    closeActionMenu();
  }
</script>

{#if activeScreen !== "devices"}
  <header class="shell-header detail">
    <div class="shell-header-group">
      <IconButton
        aria-label={$i18n.t("app.header.backToDevices")}
        class="header-icon"
        title={$i18n.t("app.header.back")}
        onClick={onBack}
      >
        <ArrowLeft aria-hidden="true" class="card-icon" />
      </IconButton>

      <div class="shell-heading">
        <h2>{selectedDeviceName ??
          (activeScreen === "settings"
            ? $i18n.t("app.settingsHeading")
            : $i18n.t("app.chartsHeading"))}</h2>
      </div>
    </div>
  </header>
{:else}
  <div class="header-actions header-actions-bottom">
    <IconButton
      aria-expanded={isActionMenuOpen}
      aria-haspopup="menu"
      aria-label={isActionMenuOpen
        ? $i18n.t("app.header.closeActions")
        : $i18n.t("app.header.openActions")}
      active={isActionMenuOpen}
      class="header-icon header-menu-toggle"
      title={$i18n.t("app.header.quickActions")}
      onClick={toggleHeaderActionMenu}
    >
      <Settings aria-hidden="true" class="card-icon" />
    </IconButton>

    {#if isActionMenuOpen}
      <div
        class="header-actions-popover header-actions-popover-bottom"
        in:fly={{ y: -10, duration: 180 }}
        out:fly={{ y: -8, duration: 140 }}
      >
        <div
          aria-label={$i18n.t("app.header.quickActions")}
          class="header-action-list panel"
          role="menu"
        >
          <IconButton
            aria-label={$i18n.t("app.header.addDevice")}
            class="header-action-item"
            role="menuitem"
            title={$i18n.t("app.header.addDevice")}
            onClick={handleOpenAddSheet}
          >
            <Plus aria-hidden="true" class="card-icon" />
          </IconButton>

          <IconButton
            aria-label={$i18n.t("theme.openPalette")}
            active={isThemePaletteOpen}
            class="header-action-item"
            role="menuitem"
            title={$i18n.t("theme.styles")}
            onClick={toggleThemePalette}
          >
            <PaletteIcon
              aria-hidden="true"
              class="card-icon header-action-icon header-action-icon-subtle"
            />
          </IconButton>

          <IconButton
            aria-label={isLocaleListOpen
              ? $i18n.t("language.closePicker")
              : $i18n.t("language.openPicker")}
            active={isLocaleListOpen}
            class="header-action-item"
            role="menuitem"
            title={$i18n.t("language.label")}
            onClick={toggleLocaleList}
          >
            <span class="header-action-locale-label">{localePreference === "auto"
              ? "A"
              : localePreference.toUpperCase()}</span>
          </IconButton>
        </div>

        {#if isLocaleListOpen}
          <div
            aria-label={$i18n.t("language.label")}
            class="header-locale-list panel"
            role="group"
          >
            {#each localeOptions as option (option.value)}
              <OptionButton
                aria-pressed={localePreference === option.value}
                active={localePreference === option.value}
                title={$i18n.t("language.useLanguage", {
                  language: option.value === "auto"
                    ? $i18n.t("language.auto")
                    : $i18n.t(`language.names.${option.value}`),
                })}
                variant="locale"
                onClick={() => handleSelectLocale(option.value)}
              >
                {option.shortLabel}
              </OptionButton>
            {/each}
          </div>
        {/if}

        <ThemePalette
          {activeTheme}
          isOpen={isThemePaletteOpen}
          mode="inline"
          onToggle={toggleThemePalette}
          onSelectTheme={handleSelectTheme}
        />
      </div>
    {/if}
  </div>
{/if}

{#if isActionMenuOpen}
  <button
    aria-label={$i18n.t("app.header.closeActions")}
    class="header-actions-backdrop"
    type="button"
    on:click={closeActionMenu}
  ></button>
{/if}
