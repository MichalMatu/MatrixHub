<script lang="ts">
  import type { HTMLButtonAttributes } from "svelte/elements";

  type OptionButtonVariant = "locale" | "shelly";

  export let active = false;
  export let variant: OptionButtonVariant = "locale";
  export let type: HTMLButtonAttributes["type"] = "button";
  export let disabled: HTMLButtonAttributes["disabled"] = false;
  export let onClick: () => void | Promise<void> = () => undefined;

  let className = "";
  export { className as class };

  $: classes = [
    variant === "locale" ? "header-locale-button" : "shelly-action",
    active ? "is-active" : "",
    className,
  ]
    .filter(Boolean)
    .join(" ");

  function handleClick() {
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
