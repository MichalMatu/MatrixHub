<script lang="ts">
  import Button from "$lib/components/ui/Button.svelte";
  import { i18n } from "$lib/i18n/store";

  export let deviceName = "";
  export let deviceAddress = "";
  export let addressError: string | undefined = undefined;
  export let isSavingDevice = false;
  export let onAddDevice: () => void | Promise<void> = () => undefined;
</script>

<form class="stack add-device-form" on:submit|preventDefault={onAddDevice}>
  <label class="field">
    <span>{$i18n.t("forms.addDevice.name")}</span>
    <input
      bind:value={deviceName}
      placeholder={$i18n.t("forms.addDevice.namePlaceholder")}
    />
  </label>

  <label class="field">
    <span>{$i18n.t("forms.addDevice.address")}</span>
    <input
      bind:value={deviceAddress}
      placeholder={$i18n.t("forms.addDevice.addressPlaceholder")}
      required
    />
    {#if addressError}
      <small class="field-help error">{addressError}</small>
    {/if}
  </label>

  <Button variant="primary" type="submit" disabled={isSavingDevice}>
    {isSavingDevice
      ? $i18n.t("forms.addDevice.saving")
      : $i18n.t("forms.addDevice.saveDevice")}
  </Button>
</form>
