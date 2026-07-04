<script lang="ts">
  import type {
    OverviewMetricId,
    OverviewMetricViewModel,
  } from "$lib/features/overview/mappers/viewModels";
  import TelemetryChartCard from "$lib/features/overview/ui/TelemetryChartCard.svelte";
  import { i18n } from "$lib/i18n/store";

  export let metrics: OverviewMetricViewModel[] = [];
  export let selectedMetricId: OverviewMetricId | null = null;
  export let showingCachedData = false;

  const METRIC_ORDER: Record<OverviewMetricId, number> = {
    co2: 0,
    temperature: 1,
    humidity: 2,
  };

  function orderMetrics(input: OverviewMetricViewModel[]) {
    return [...input].sort(
      (left, right) => METRIC_ORDER[left.id] - METRIC_ORDER[right.id],
    );
  }

  $: orderedMetrics = orderMetrics(metrics);
</script>

<section class="panel telemetry-charts-panel">
  {#if showingCachedData}
    <p class="panel-copy cache-note">{$i18n.t("common.cachedUntilLiveRefresh")}</p>
  {/if}

  {#if orderedMetrics.length > 0}
    <div class="telemetry-chart-list">
      {#each orderedMetrics as metric (metric.id)}
        <TelemetryChartCard
          {metric}
          highlighted={selectedMetricId !== null && metric.id === selectedMetricId}
        />
      {/each}
    </div>
  {:else}
    <div class="telemetry-chart-empty-state">
      {$i18n.t("deviceCard.sensors.waitingForData")}
    </div>
  {/if}
</section>

<style>
  .telemetry-charts-panel {
    display: grid;
    gap: 0.55rem;
  }

  .telemetry-chart-list {
    display: grid;
    gap: 0.45rem;
  }

  .telemetry-chart-empty-state {
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 10rem;
    border: 1px dashed var(--line);
    border-radius: 18px;
    color: var(--muted);
    text-align: center;
    padding: 1rem;
  }
</style>
