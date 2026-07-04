<script lang="ts">
  import { createEventDispatcher } from "svelte";
  import { calculateSparklineGeometry } from "./sparklineGeometry";

  export let id: string;
  export let data: number[] = [];
  export let width = 168;
  export let height = 42;
  export let color = "currentColor";
  export let strokeWidth = 2;
  export let domainMin: number | undefined = undefined;
  export let domainMax: number | undefined = undefined;
  export let padding: number | undefined = undefined;
  export let smoothingTension = 4;

  const dispatch = createEventDispatcher<{
    hover: { value: number | null; index: number | null };
  }>();

  let svgElement: SVGSVGElement;
  let hoverIndex: number | null = null;

  $: gradientId = `sparkline-gradient-${id}`;
  $: chartData = calculateSparklineGeometry({
    data,
    width,
    height,
    strokeWidth,
    domainMin,
    domainMax,
    padding,
    smoothingTension,
  });

  function emitHover(index: number | null) {
    dispatch("hover", {
      value: index === null ? null : data[index] ?? null,
      index,
    });
  }

  function handleMouseMove(event: MouseEvent) {
    if (!svgElement || data.length < 2) {
      return;
    }

    const rect = svgElement.getBoundingClientRect();
    const widthPx = rect.width || 1;
    const relativeX = event.clientX - rect.left;
    const index = Math.min(
      Math.max(0, Math.floor((relativeX / widthPx) * data.length)),
      data.length - 1,
    );

    if (hoverIndex === index) {
      return;
    }

    hoverIndex = index;
    emitHover(index);
  }

  function handleMouseLeave() {
    if (hoverIndex === null) {
      return;
    }

    hoverIndex = null;
    emitHover(null);
  }
</script>

<svg
  bind:this={svgElement}
  {width}
  {height}
  viewBox={`0 0 ${width} ${height}`}
  class="sparkline-svg"
  preserveAspectRatio="xMaxYMid meet"
  aria-hidden="true"
  on:mousemove={handleMouseMove}
  on:mouseleave={handleMouseLeave}
>
  <defs>
    <linearGradient id={gradientId} x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color={color} stop-opacity="0.28" />
      <stop offset="100%" stop-color={color} stop-opacity="0.02" />
    </linearGradient>
  </defs>

  {#if chartData.smoothFillPath}
    <path d={chartData.smoothFillPath} fill={`url(#${gradientId})`} />
  {:else if chartData.fillPoints}
    <polygon points={chartData.fillPoints} fill={`url(#${gradientId})`} />
  {/if}

  {#if chartData.smoothPath}
    <path
      d={chartData.smoothPath}
      fill="none"
      stroke={color}
      stroke-width={strokeWidth}
      stroke-linecap="round"
      stroke-linejoin="round"
      vector-effect="non-scaling-stroke"
      class="sparkline-line"
    />
  {:else if chartData.points}
    <polyline
      points={chartData.points}
      fill="none"
      stroke={color}
      stroke-width={strokeWidth}
      stroke-linecap="round"
      stroke-linejoin="round"
      vector-effect="non-scaling-stroke"
      class="sparkline-line"
    />
  {/if}

  {#if chartData.lastPoint}
    <circle
      cx={chartData.lastPoint.x}
      cy={chartData.lastPoint.y}
      r={strokeWidth + 1}
      fill={color}
      class="sparkline-endpoint"
    />
  {/if}
</svg>

<style>
  .sparkline-svg {
    display: block;
    width: 100%;
    height: auto;
    max-width: 100%;
    overflow: visible;
    cursor: crosshair;
  }

  .sparkline-line {
    transition: stroke-dashoffset 0.24s ease-out;
  }

  .sparkline-endpoint {
    filter: drop-shadow(0 0 2px color-mix(in oklab, currentColor 56%, transparent));
  }
</style>
