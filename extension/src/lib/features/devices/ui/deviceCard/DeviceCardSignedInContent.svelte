<script lang="ts">
  import type {
    DeviceSession,
    MatrixSettings,
    ShellyDevice,
  } from "@matrixhub/device-sdk";
  import type {
    OverviewMetricId,
    OverviewMetricViewModel,
  } from "$lib/features/overview/mappers/viewModels";
  import type { BleThermometerViewModel } from "./model";
  import { formatBrightnessValue } from "./model";
  import { i18n } from "$lib/i18n/store";
  import MatrixBrightnessQuickControl from "$lib/features/matrix/ui/MatrixBrightnessQuickControl.svelte";
  import TelemetryMetricCard from "$lib/features/devices/ui/TelemetryMetricCard.svelte";
  import DeviceCardBleSection from "./DeviceCardBleSection.svelte";
  import DeviceCardShellySection from "./DeviceCardShellySection.svelte";

  export let deviceId: string;
  export let session: DeviceSession;
  export let overviewMetrics: OverviewMetricViewModel[] = [];
  export let bleThermometers: BleThermometerViewModel[] = [];
  export let shellyDevices: ShellyDevice[] = [];
  export let matrixSettings: MatrixSettings | null = null;
  export let pendingShellyDeviceId: string | null = null;
  export let showingCachedData = false;
  export let isSavingMatrixSettings = false;
  export let bluetoothOpen = true;
  export let shellyOpen = true;
  export let onSetShellyState: (
    shellyDeviceId: string,
    turnOn: boolean,
  ) => void | Promise<void> = () => undefined;
  export let onSaveMatrixSettings: (
    settings: Partial<MatrixSettings>,
    options?: { notify?: boolean },
  ) => void | Promise<void> = () => undefined;
  export let onToggleBluetooth: () => void | Promise<void> = () => undefined;
  export let onToggleShelly: () => void | Promise<void> = () => undefined;
  export let onOpenCharts: (
    metricId: OverviewMetricId,
  ) => void | Promise<void> = () => undefined;
</script>

<div class:with-cached-toast={showingCachedData} class="device-card-body">
  <div class="mini-metrics">
    {#if overviewMetrics.length > 0}
      {#each overviewMetrics as metric (metric.id)}
        <TelemetryMetricCard
          {metric}
          onOpen={() => onOpenCharts(metric.id)}
        />
      {/each}
    {:else}
      <article class="mini-metric empty">
        <span class="mini-metric-label">{$i18n.t("deviceCard.sensors.title")}</span>
        <strong class="mini-metric-value">{$i18n.t("deviceCard.sensors.waitingForData")}</strong>
      </article>
    {/if}
  </div>

  {#if matrixSettings && session.admin}
    <section class="device-card-section matrix-inline-section">
      <div class="matrix-inline-head">
        <span class="section-kicker">{$i18n.t("deviceCard.sections.led")}</span>

        <MatrixBrightnessQuickControl
          settings={matrixSettings}
          saving={isSavingMatrixSettings}
          onSave={onSaveMatrixSettings}
        />

        <div class="matrix-quickbar-meta">
          <span class="matrix-quickbar-value">
            {formatBrightnessValue(matrixSettings.brightness)}
          </span>
        </div>
      </div>
    </section>
  {/if}

  {#if bleThermometers.length > 0}
    <DeviceCardBleSection
      {deviceId}
      {bleThermometers}
      open={bluetoothOpen}
      onToggle={onToggleBluetooth}
    />
  {/if}

  {#if shellyDevices.length > 0}
    <DeviceCardShellySection
      {deviceId}
      {session}
      {shellyDevices}
      {pendingShellyDeviceId}
      open={shellyOpen}
      onToggle={onToggleShelly}
      {onSetShellyState}
    />
  {/if}
</div>

{#if showingCachedData}
  <p aria-live="polite" class="device-card-toast" role="status">
    {$i18n.t("deviceCard.cachedNote")}
  </p>
{/if}
