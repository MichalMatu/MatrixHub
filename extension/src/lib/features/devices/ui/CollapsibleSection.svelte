<script lang="ts">
  import ChevronDown from "~icons/tabler/chevron-down";

  export let label: string;
  export let open = true;
  export let sectionId: string;
  export let onToggle: () => void | Promise<void> = () => undefined;
</script>

<section class="device-card-section collapsible-section" data-open={open}>
  <button
    aria-controls={sectionId}
    aria-expanded={open}
    class="collapsible-section-toggle"
    type="button"
    on:click={onToggle}
  >
    <div class="device-card-subhead">
      <span class="section-kicker">{label}</span>

      <div class="collapsible-section-meta">
        <slot name="meta" />
        <ChevronDown aria-hidden="true" class="collapsible-section-chevron" />
      </div>
    </div>
  </button>

  <div class="collapsible-section-body" hidden={!open} id={sectionId}>
    {#if open}
      <slot />
    {/if}
  </div>
</section>
