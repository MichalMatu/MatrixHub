<script lang="ts">
  import Sparkline from "$lib/features/devices/ui/Sparkline.svelte";
  import type { OverviewMetricViewModel } from "$lib/features/overview/mappers/viewModels";
  import { buildMetricChartScale } from "$lib/features/overview/chartScale";
  import {
    formatCo2Value,
    formatHumidityValue,
    formatTemperatureValue,
  } from "$lib/i18n/formatters";
  import { i18n } from "$lib/i18n/store";

  export let metric: OverviewMetricViewModel;
  export let highlighted = false;

  let hoveredValue: number | null = null;
  let hoveredIndex: number | null = null;

  $: scale = buildMetricChartScale(metric);
  $: stats = buildStats(metric.history.length > 0 ? metric.history : [metric.value]);
  $: activeValue = hoveredValue ?? metric.value;
  $: activeTime =
    hoveredIndex === null ? null : formatTimestamp(metric.timestamps[hoveredIndex] ?? null);

  function buildStats(values: number[]) {
    const min = Math.min(...values);
    const max = Math.max(...values);
    return {
      min,
      max,
    };
  }

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

  function formatMetricStatValue(value: number) {
    switch (metric.id) {
      case "temperature":
      case "humidity":
        return $i18n.formatDecimal(value, 1);
      default:
        return $i18n.formatInteger(Math.round(value));
    }
  }

  function formatTimestamp(timestamp: number | null) {
    if (timestamp === null || !Number.isFinite(timestamp)) {
      return null;
    }

    const timestampMs =
      timestamp >= 100_000_000_000 ? timestamp : timestamp * 1000;

    return new Intl.DateTimeFormat($i18n.locale, {
      hour: "2-digit",
      minute: "2-digit",
    }).format(new Date(timestampMs));
  }

  function handleHover(
    event: CustomEvent<{ value: number | null; index: number | null }>,
  ) {
    hoveredValue = event.detail.value;
    hoveredIndex = event.detail.index;
  }
</script>

<article class:highlighted class="telemetry-chart-card">
  <div class="telemetry-chart-card-head">
    {#if activeTime}
      <small class="telemetry-chart-time">{activeTime}</small>
    {/if}

    <div class="telemetry-chart-card-copy">
      <span class="section-kicker">{metric.label}</span>
      <div class="telemetry-chart-card-summary">
        <strong>{formatMetricValue(activeValue)}</strong>

        <div class="telemetry-chart-side">
          <div class="telemetry-chart-side-stats">
            <span>
              <b>{$i18n.t("overview.charts.min")}</b>
              <strong>{formatMetricStatValue(stats.min)}</strong>
            </span>
            <span>
              <b>{$i18n.t("overview.charts.max")}</b>
              <strong>{formatMetricStatValue(stats.max)}</strong>
            </span>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="telemetry-chart-shell">
    {#if metric.history.length > 1}
      <div class="telemetry-chart-plot">
        <div class="telemetry-chart-grid" aria-hidden="true">
          <span></span>
          <span></span>
          <span></span>
        </div>

        <Sparkline
          id={`charts-${metric.id}`}
          data={metric.history}
          color={metric.chartColor}
          width={300}
          height={100}
          strokeWidth={2}
          domainMin={scale.min}
          domainMax={scale.max}
          on:hover={handleHover}
        />
      </div>
    {:else}
      <div class="telemetry-chart-empty">
        {$i18n.t("overview.realtime.waitingForData")}
      </div>
    {/if}
  </div>
</article>

<style>
  .telemetry-chart-card {
    display: grid;
    gap: 0.46rem;
    padding: 0.72rem 0.72rem 0.66rem;
    border: 1px solid var(--line);
    border-radius: 18px;
    background:
      linear-gradient(
        180deg,
        color-mix(in oklab, var(--panel-strong) 92%, transparent),
        color-mix(in oklab, var(--panel) 86%, transparent)
      );
  }

  .telemetry-chart-card.highlighted {
    border-color: color-mix(in oklab, var(--accent) 28%, var(--line) 72%);
    box-shadow: 0 12px 28px color-mix(in oklab, var(--accent) 10%, transparent);
  }

  .telemetry-chart-card-head {
    position: relative;
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    gap: 0.55rem;
    padding-right: 3.5rem;
  }

  .telemetry-chart-card-copy {
    display: grid;
    gap: 0.14rem;
    min-width: 0;
  }

  .telemetry-chart-card-summary {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 0.55rem;
  }

  .telemetry-chart-card-copy strong {
    font-size: 1rem;
    line-height: 1.2;
    white-space: nowrap;
  }

  .telemetry-chart-time {
    position: absolute;
    top: 0;
    right: 0;
    color: var(--muted);
    font-size: 0.78rem;
    line-height: 1.1;
    white-space: nowrap;
  }

  .telemetry-chart-side {
    display: grid;
    justify-items: end;
    gap: 0.08rem;
    flex: 0 0 auto;
    min-width: 0;
  }

  .telemetry-chart-side-stats {
    display: flex;
    flex-wrap: wrap;
    justify-content: flex-end;
    gap: 0.34rem;
  }

  .telemetry-chart-side-stats span {
    color: var(--muted);
    display: inline-flex;
    align-items: baseline;
    gap: 0.18rem;
    font-size: 0.64rem;
    line-height: 1.15;
    white-space: nowrap;
  }

  .telemetry-chart-side-stats b {
    color: var(--text);
    font-size: 0.6rem;
    font-weight: 700;
    letter-spacing: 0.04em;
    margin-right: 0.18rem;
    text-transform: uppercase;
  }

  .telemetry-chart-side-stats strong {
    color: var(--text);
    font-size: 0.78rem;
    font-weight: 700;
    line-height: 1;
  }

  .telemetry-chart-shell {
    min-width: 0;
  }

  .telemetry-chart-plot {
    position: relative;
    min-width: 0;
    aspect-ratio: 3 / 1;
    padding: 0.04rem 0 0;
  }

  .telemetry-chart-grid {
    position: absolute;
    inset: 0;
    display: grid;
    pointer-events: none;
  }

  .telemetry-chart-grid span {
    border-top: 1px dashed color-mix(in oklab, var(--line) 72%, transparent);
  }

  .telemetry-chart-grid span:last-child {
    border-bottom: 1px dashed color-mix(in oklab, var(--line) 72%, transparent);
    border-top: none;
  }

  .telemetry-chart-empty {
    display: flex;
    align-items: center;
    justify-content: center;
    aspect-ratio: 3 / 1;
    border: 1px dashed var(--line);
    border-radius: 16px;
    color: var(--muted);
    font-size: 0.8rem;
    text-align: center;
    padding: 0.9rem;
  }
</style>
