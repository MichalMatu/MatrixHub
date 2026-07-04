<script lang="ts">
  import type { DeviceSession, ShellyDevice } from "@matrixhub/device-sdk";
  import OptionButton from "$lib/components/ui/OptionButton.svelte";
  import CollapsibleSection from "$lib/features/devices/ui/CollapsibleSection.svelte";
  import { i18n } from "$lib/i18n/store";
  import {
    getShellyStateCopy,
    getShellyStateTone,
  } from "./model";

  export let deviceId: string;
  export let session: DeviceSession;
  export let shellyDevices: ShellyDevice[] = [];
  export let pendingShellyDeviceId: string | null = null;
  export let open = true;
  export let onToggle: () => void | Promise<void> = () => undefined;
  export let onSetShellyState: (
    shellyDeviceId: string,
    turnOn: boolean,
  ) => void | Promise<void> = () => undefined;

  $: shellySummary = $i18n.t("deviceCard.shelly.relayCount", {
    count: shellyDevices.length,
    value: $i18n.formatInteger(shellyDevices.length),
  });
</script>

<CollapsibleSection
  label={$i18n.t("deviceCard.sections.shelly")}
  {open}
  sectionId={`${deviceId}-shelly-section`}
  onToggle={onToggle}
>
  <small slot="meta">{shellySummary}</small>
  <div class="shelly-relay-list">
    {#each shellyDevices as shelly (shelly.id)}
      <article class="shelly-relay-card">
        <div class="shelly-relay-head">
          <div class="shelly-relay-title">
            <strong>{shelly.name}</strong>
            <small class="shelly-state" data-state={getShellyStateTone(pendingShellyDeviceId, shelly)}>
              {getShellyStateCopy($i18n, pendingShellyDeviceId, shelly)}
            </small>
          </div>

          <div class="shelly-relay-actions">
            <OptionButton
              active={shelly.isOn}
              aria-pressed={shelly.isOn}
              disabled={pendingShellyDeviceId === shelly.id || !session.admin}
              title={session.admin
                ? $i18n.t("deviceCard.shelly.turnOn")
                : $i18n.t("deviceCard.shelly.adminRequired")}
              variant="shelly"
              onClick={() => onSetShellyState(shelly.id, true)}
            >
              {$i18n.t("deviceCard.shelly.on")}
            </OptionButton>
            <OptionButton
              active={!shelly.isOn}
              aria-pressed={!shelly.isOn}
              disabled={pendingShellyDeviceId === shelly.id || !session.admin}
              title={session.admin
                ? $i18n.t("deviceCard.shelly.turnOff")
                : $i18n.t("deviceCard.shelly.adminRequired")}
              variant="shelly"
              onClick={() => onSetShellyState(shelly.id, false)}
            >
              {$i18n.t("deviceCard.shelly.off")}
            </OptionButton>
          </div>
        </div>
      </article>
    {/each}
  </div>
</CollapsibleSection>
