<script lang="ts">
  import { fly } from "svelte/transition";
  import X from "~icons/tabler/x";
  import IconButton from "$lib/components/ui/IconButton.svelte";
  import AddDevicePanel from "$lib/features/devices/ui/AddDevicePanel.svelte";
  import { i18n } from "$lib/i18n/store";

  export let isOpen = false;
  export let deviceName = "";
  export let deviceAddress = "";
  export let addressError: string | undefined = undefined;
  export let isSavingDevice = false;
  export let onAddDevice: () => void | Promise<void> = () => undefined;
  export let onClose: () => void | Promise<void> = () => undefined;
</script>

{#if isOpen}
  <div
    aria-hidden="true"
    class="sheet-backdrop"
    in:fly={{ y: 18, duration: 180 }}
    out:fly={{ y: 18, duration: 140 }}
    on:click={onClose}
  ></div>

  <div
    aria-label={$i18n.t("app.addDeviceSheet.dialogLabel")}
    aria-modal="true"
    class="panel add-sheet"
    role="dialog"
    in:fly={{ y: 18, duration: 180 }}
    out:fly={{ y: 18, duration: 140 }}
  >
    <div class="sheet-head">
      <h2>{$i18n.t("app.addDeviceSheet.title")}</h2>

      <IconButton
        aria-label={$i18n.t("app.addDeviceSheet.closeLabel")}
        title={$i18n.t("app.addDeviceSheet.close")}
        onClick={onClose}
      >
        <X aria-hidden="true" class="card-icon" />
      </IconButton>
    </div>

    <AddDevicePanel
      bind:deviceName
      bind:deviceAddress
      {addressError}
      {isSavingDevice}
      onAddDevice={onAddDevice}
    />
  </div>
{/if}
