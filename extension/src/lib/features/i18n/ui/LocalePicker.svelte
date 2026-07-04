<script lang="ts">
  import { fly } from "svelte/transition";
  import Check from "~icons/tabler/check";
  import { i18n } from "$lib/i18n/store";
  import type { LocalePreference, SupportedLocale } from "$lib/i18n/locales";

  export let isOpen = false;
  export let activeLocale: SupportedLocale;
  export let preference: LocalePreference;
  export let onSelectPreference: (
    preference: LocalePreference,
  ) => void | Promise<void> = () => undefined;

  function getLocaleLabel(locale: SupportedLocale) {
    switch (locale) {
      case "pl":
        return $i18n.t("language.names.pl");
      default:
        return $i18n.t("language.names.en");
    }
  }

  function getPreferenceLabel(value: LocalePreference) {
    return value === "auto" ? $i18n.t("language.auto") : getLocaleLabel(value);
  }

  function getOptionHint(value: LocalePreference) {
    return value === "auto" ? $i18n.t("language.autoHint") : "";
  }

  function getOptionShortLabel(value: LocalePreference) {
    return value === "auto" ? "A" : value.toUpperCase();
  }

  $: activeLabel =
    preference === "auto"
      ? $i18n.t("language.activeAuto", {
          language: getLocaleLabel(activeLocale),
        })
      : getPreferenceLabel(preference);

  $: localeOptions = (["auto", "en", "pl"] as const).map((value) => ({
    value,
    shortLabel: getOptionShortLabel(value),
    label: getPreferenceLabel(value),
    hint: getOptionHint(value),
  }));
</script>

{#if isOpen}
  <section
    aria-label={$i18n.t("language.pickerLabel")}
    class="locale-picker panel"
    in:fly={{ y: -8, duration: 180 }}
    out:fly={{ y: -8, duration: 140 }}
  >
    <div class="locale-picker-head">
      <strong>{$i18n.t("language.label")}</strong>
      <span class="locale-picker-active">{activeLabel}</span>
    </div>

    <div class="locale-option-list">
      {#each localeOptions as option (option.value)}
        <button
          aria-pressed={option.value === preference}
          class:selected={option.value === preference}
          class="locale-option"
          title={$i18n.t("language.useLanguage", {
            language: option.label,
          })}
          type="button"
          on:click={() => onSelectPreference(option.value)}
        >
          <span class="locale-option-chip">{option.shortLabel}</span>

          <span class="locale-option-copy">
            <strong>{option.label}</strong>
            {#if option.hint}
              <small>{option.hint}</small>
            {/if}
          </span>

          {#if option.value === preference}
            <span class="locale-option-check">
              <Check aria-hidden="true" />
            </span>
          {/if}
        </button>
      {/each}
    </div>
  </section>
{/if}

<style>
  .locale-picker {
    width: min(14.5rem, calc(100vw - 2rem));
    max-width: calc(100vw - 2rem);
    display: grid;
    gap: 0.6rem;
    padding: 0.7rem;
  }

  .locale-picker-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.6rem;
  }

  .locale-picker-head strong {
    font-size: 0.78rem;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }

  .locale-picker-active {
    color: var(--muted);
    font-size: 0.72rem;
    font-weight: 700;
    letter-spacing: 0.03em;
    text-align: right;
  }

  .locale-option-list {
    display: grid;
    gap: 0.45rem;
  }

  .locale-option {
    position: relative;
    display: grid;
    grid-template-columns: auto minmax(0, 1fr);
    align-items: center;
    gap: 0.6rem;
    width: 100%;
    padding: 0.56rem 0.68rem;
    border: 1px solid var(--line);
    border-radius: 14px;
    background: var(--panel-strong);
    color: inherit;
    text-align: left;
  }

  .locale-option.selected {
    border-color: color-mix(in oklab, var(--accent) 36%, var(--line) 64%);
    background: linear-gradient(
      180deg,
      color-mix(in oklab, var(--accent-soft) 72%, var(--panel-strong) 28%),
      var(--panel-strong)
    );
  }

  .locale-option-copy {
    display: grid;
    gap: 0.14rem;
    min-width: 0;
    padding-right: 1.2rem;
  }

  .locale-option-copy strong {
    color: var(--text);
    font-size: 0.8rem;
    line-height: 1.2;
  }

  .locale-option-copy small {
    color: var(--muted);
    font-size: 0.68rem;
    line-height: 1.3;
  }

  .locale-option-chip {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    min-width: 2.5rem;
    padding: 0.26rem 0.45rem;
    border: 1px solid color-mix(in oklab, var(--line) 82%, white 18%);
    border-radius: 999px;
    background: color-mix(in oklab, var(--panel) 90%, transparent);
    color: var(--muted);
    font-size: 0.62rem;
    font-weight: 800;
    letter-spacing: 0.06em;
    line-height: 1;
    text-transform: uppercase;
  }

  .locale-option.selected .locale-option-chip {
    border-color: color-mix(in oklab, var(--accent) 34%, var(--line) 66%);
    color: var(--accent);
  }

  .locale-option-check {
    position: absolute;
    top: 0.58rem;
    right: 0.58rem;
    width: 0.9rem;
    height: 0.9rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    color: var(--accent);
    flex: 0 0 auto;
  }
</style>
