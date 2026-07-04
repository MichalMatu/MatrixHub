<script lang="ts">
  import type { HTMLButtonAttributes } from "svelte/elements";

  type IconButtonTone = "default" | "danger";

  export let active = false;
  export let tone: IconButtonTone = "default";
  export let stopPropagation = false;
  export let type: HTMLButtonAttributes["type"] = "button";
  export let disabled: HTMLButtonAttributes["disabled"] = false;
  export let onClick: () => void | Promise<void> = () => undefined;

  let className = "";
  export { className as class };

  $: classes = ["icon-button", active ? "is-active" : "", tone === "danger" ? "danger" : "", className]
    .filter(Boolean)
    .join(" ");

  function handleClick(event: MouseEvent) {
    if (stopPropagation) {
      event.stopPropagation();
    }

    void onClick();
  }
</script>

<button
  {...$$restProps}
  {type}
  {disabled}
  class={classes}
  on:click={handleClick}
>
  <slot />
</button>
