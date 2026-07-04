<script lang="ts">
  import {
    formatCo2Value,
    formatHumidityValue,
    formatTemperatureValue,
  } from "$lib/i18n/formatters";
  import { i18n } from "$lib/i18n/store";
  import type { OverviewMetricViewModel } from "$lib/features/overview/mappers/viewModels";

  export let metric: OverviewMetricViewModel;
  export let onOpen: () => void | Promise<void> = () => undefined;

  $: displayValue = formatMetricValue(metric.value);
  $: displayParts = splitMetricValue(displayValue);

  function formatMetricValue(value: number) {
    switch (metric.id) {
      case "temperature":
        return formatTemperatureValue($i18n, value);
      case "humidity":
        return formatHumidityValue($i18n, value);
      default:
        return formatCo2Value($i18n, value);
    }
  }

  function splitMetricValue(value: string) {
    const match = value.match(/^(.+?)(?:\s+(\S.*))?$/);

    return {
      amount: match?.[1] ?? value,
      unit: match?.[2] ?? "",
    };
  }
</script>

<button
  type="button"
  class="mini-metric mini-metric-button"
  style={`--metric-color: ${metric.chartColor};`}
  data-metric-id={metric.id}
  aria-label={$i18n.t("deviceCard.sensors.openCharts", { label: metric.label })}
  title={$i18n.t("deviceCard.sensors.openCharts", { label: metric.label })}
  on:click={onOpen}
>
  <span class="mini-metric-top">
    <span class="mini-metric-label">{metric.label}</span>
  </span>

  <span class="mini-metric-reading">
    <strong class="mini-metric-value">{displayParts.amount}</strong>
    {#if displayParts.unit}
      <span class="mini-metric-unit">{displayParts.unit}</span>
    {/if}
  </span>
</button>

<style>
  .mini-metric-button {
    position: relative;
    width: 100%;
    text-align: left;
    border: 1px solid var(--line);
    overflow: hidden;
    cursor: pointer;
    transition:
      transform 0.18s ease,
      border-color 0.18s ease,
      box-shadow 0.18s ease,
      background 0.18s ease;
    background:
      linear-gradient(
        180deg,
        color-mix(in oklab, var(--metric-color) 10%, var(--panel-strong) 90%),
        var(--panel-strong)
      );
  }

  .mini-metric-button::before {
    content: "";
    position: absolute;
    inset: 0;
    background:
      radial-gradient(
        circle at top right,
        color-mix(in oklab, var(--metric-color) 16%, transparent),
        transparent 52%
      ),
      linear-gradient(
        180deg,
        color-mix(in oklab, var(--metric-color) 6%, transparent),
        transparent 68%
      );
    pointer-events: none;
  }

  .mini-metric-button > * {
    position: relative;
    z-index: 1;
  }

  .mini-metric-button:hover:not(:disabled) {
    border-color: color-mix(in oklab, var(--metric-color) 26%, var(--line) 74%);
    box-shadow: 0 12px 24px color-mix(in oklab, var(--metric-color) 12%, transparent);
    transform: translateY(-1px);
  }

  .mini-metric-button:focus-visible {
    outline: 2px solid color-mix(in oklab, var(--metric-color) 28%, white 72%);
    outline-offset: 2px;
  }

  .mini-metric-button:active:not(:disabled) {
    transform: translateY(0);
  }

  .mini-metric-top {
    display: flex;
    align-items: flex-start;
    gap: 0.3rem;
    min-width: 0;
  }

  .mini-metric-reading {
    display: flex;
    align-items: baseline;
    gap: 0.28rem;
    flex-wrap: nowrap;
    min-width: 0;
  }

  .mini-metric-unit {
    color: color-mix(in oklab, var(--text) 72%, var(--muted) 28%);
    font-size: 0.66rem;
    font-weight: 700;
    line-height: 1.2;
    white-space: nowrap;
  }
</style>
