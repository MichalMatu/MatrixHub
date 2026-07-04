<script lang="ts">
  import type { HTMLButtonAttributes } from "svelte/elements";

  type ButtonVariant = "primary" | "ghost";
  type ButtonTone = "default" | "danger";

  export let variant: ButtonVariant = "primary";
  export let tone: ButtonTone = "default";
  export let type: HTMLButtonAttributes["type"] = "button";
  export let disabled: HTMLButtonAttributes["disabled"] = false;
  export let onClick: () => void | Promise<void> = () => undefined;

  let className = "";
  export { className as class };

  $: classes = [
    variant === "primary" ? "primary" : "ghost",
    variant === "ghost" && tone === "danger" ? "danger" : "",
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
