<script lang="ts">
  import Button from "$lib/components/ui/Button.svelte";
  import type { DeviceSettingsViewModel } from "$lib/features/overview/mappers/viewModels";
  import { i18n } from "$lib/i18n/store";

  export let heading = "";
  export let viewModel: DeviceSettingsViewModel;
  export let showingCachedData = false;
  export let canSignOut = false;
  export let onOpenDevice: () => void | Promise<void> = () => undefined;
  export let onLogout: () => void | Promise<void> = () => undefined;
</script>

<section class="panel settings-shell">
  <div class="section-head compact-head">
    <h2>{heading || $i18n.t("settingsPanel.heading")}</h2>
  </div>

  {#if showingCachedData}
    <p class="panel-copy cache-note">{$i18n.t("common.cachedUntilLiveRefresh")}</p>
  {/if}

  <div class="settings-fact-row" aria-label={$i18n.t("settingsPanel.detailsLabel")}>
    {#each viewModel.deviceItems as detail (detail.id)}
      <article class="settings-fact-chip">
        {#if detail.label}
          <span>{detail.label}</span>
        {/if}
        <strong>{detail.value}</strong>
      </article>
    {/each}
  </div>

  <div class="settings-status-list" aria-label={$i18n.t("settingsPanel.statusLabel")}>
    {#each viewModel.statusItems as detail (detail.id)}
      <article class="settings-status-row">
        <span>{detail.label}</span>
        <strong>{detail.value}</strong>
      </article>
    {/each}
  </div>

  <div class="button-row settings-actions-compact">
    <Button variant="ghost" onClick={onOpenDevice}>
      {$i18n.t("common.open")}
    </Button>
    {#if canSignOut}
      <Button variant="ghost" tone="danger" onClick={onLogout}>
        {$i18n.t("settingsPanel.signOut")}
      </Button>
    {/if}
  </div>
</section>
