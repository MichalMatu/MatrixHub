<script lang="ts">
  import { fly } from "svelte/transition";
  import type { ToastItem } from "$lib/features/toasts/toastQueue";
  import { i18n } from "$lib/i18n/store";

  export let isBootstrapping = false;
  export let toasts: ToastItem[] = [];
</script>

<div aria-live="polite" class="toast-region">
  {#if isBootstrapping}
    <div
      class="toast toast-status"
      in:fly={{ x: -18, duration: 180 }}
      out:fly={{ x: -18, duration: 140 }}
    >
      <div class="toast-copy">
        <strong>{$i18n.t("app.startup.title")}</strong>
        <p>{$i18n.t("app.startup.copy")}</p>
      </div>
    </div>
  {/if}

  {#each toasts as toast (toast.id)}
    <div
      class={`toast toast-${toast.tone}`}
      role={toast.tone === "error" ? "alert" : "status"}
      in:fly={{ x: -18, duration: 180 }}
      out:fly={{ x: -18, duration: 140 }}
    >
      <div class="toast-copy">
        <strong>{toast.tone === "error"
          ? $i18n.t("app.toast.problem")
          : $i18n.t("app.toast.done")}</strong>
        <p>{toast.message}</p>
      </div>
    </div>
  {/each}
</div>
