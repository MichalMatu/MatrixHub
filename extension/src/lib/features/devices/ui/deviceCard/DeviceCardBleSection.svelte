<script lang="ts">
  import { i18n } from "$lib/i18n/store";
  import CollapsibleSection from "$lib/features/devices/ui/CollapsibleSection.svelte";
  import type { BleThermometerViewModel } from "./model";
  import {
    formatBleBattery,
    formatBleHumidity,
    formatBleTemperature,
    getBleIndicatorLabel,
    getBleIndicatorState,
    getBleSummaryLabel,
    getBleSummaryReading,
  } from "./model";

  export let deviceId: string;
  export let bleThermometers: BleThermometerViewModel[] = [];
  export let open = true;
  export let onToggle: () => void | Promise<void> = () => undefined;
</script>

<CollapsibleSection
  label={$i18n.t("deviceCard.sections.bluetooth")}
  {open}
  sectionId={`${deviceId}-bluetooth-section`}
  onToggle={onToggle}
>
  <div class="ble-thermometer-summary" slot="meta">
    {#if !open}
      {#each bleThermometers as thermometer (thermometer.id)}
        <span
          class="ble-thermometer-summary-badge"
          data-state={getBleIndicatorState(thermometer.reading?.last_seen)}
          aria-label={getBleSummaryLabel($i18n, thermometer.label, thermometer.reading)}
          title={getBleSummaryLabel($i18n, thermometer.label, thermometer.reading)}
        >
          <span class="ble-thermometer-summary-reading">
            {getBleSummaryReading($i18n, thermometer.reading)}
          </span>
        </span>
      {/each}
    {/if}
  </div>
  <div class="ble-thermometer-list">
    {#each bleThermometers as thermometer (thermometer.id)}
      <article class="ble-thermometer-card">
        <div class="ble-thermometer-head">
          <div class="ble-thermometer-title">
            <span
              class="ble-thermometer-dot"
              data-state={getBleIndicatorState(thermometer.reading?.last_seen)}
              role="img"
              aria-label={getBleIndicatorLabel($i18n, thermometer.reading?.last_seen)}
              title={getBleIndicatorLabel($i18n, thermometer.reading?.last_seen)}
            ></span>
            <strong>{thermometer.label}</strong>
          </div>
          {#if thermometer.label !== thermometer.mac}
            <small>{thermometer.mac}</small>
          {/if}
        </div>

        <div class="ble-thermometer-badges">
          <article class="ble-thermometer-badge">
            <span>{$i18n.t("deviceCard.ble.temperatureBadge")}</span>
            <strong>{formatBleTemperature($i18n, thermometer.reading?.temp)}</strong>
          </article>
          <article class="ble-thermometer-badge">
            <span>{$i18n.t("deviceCard.ble.humidityBadge")}</span>
            <strong>{formatBleHumidity($i18n, thermometer.reading?.humid)}</strong>
          </article>
          <article class="ble-thermometer-badge">
            <span>{$i18n.t("deviceCard.ble.batteryBadge")}</span>
            <strong>{formatBleBattery($i18n, thermometer.reading?.batt)}</strong>
          </article>
        </div>
      </article>
    {/each}
  </div>
</CollapsibleSection>
