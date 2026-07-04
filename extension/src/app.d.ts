declare module "*.svelte" {
  import type { SvelteComponentTyped } from "svelte";

  export default class SvelteComponent extends SvelteComponentTyped<
    Record<string, unknown>,
    Record<string, unknown>,
    Record<string, unknown>
  > {}
}

/// <reference types="unplugin-icons/types/svelte" />
