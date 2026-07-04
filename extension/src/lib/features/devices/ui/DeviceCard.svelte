<script lang="ts">
  import type {
    BleStatus,
    DeviceRecord,
    DeviceSession,
    MatrixSettings,
    ShellyDevice,
  } from "@matrixhub/device-sdk";
  import type { RealtimeConnectionState } from "$lib/features/realtime/socket/deviceOverviewSocket";
  import type {
    OverviewMetricId,
    OverviewMetricViewModel,
  } from "$lib/features/overview/mappers/viewModels";
  import type {
    DeviceCardSectionKey,
    DeviceCardSectionState,
  } from "$lib/features/sidepanel/state/deviceCardSections";
  import DeviceCardAuthForm from "./deviceCard/DeviceCardAuthForm.svelte";
  import DeviceCardHeader from "./deviceCard/DeviceCardHeader.svelte";
  import DeviceCardSignedInContent from "./deviceCard/DeviceCardSignedInContent.svelte";
  import { createBleThermometers } from "./deviceCard/model";

  export let device: DeviceRecord;
  export let session: DeviceSession | null = null;
  export let isSelected = false;
  export let isSigningIn = false;
  export let username = "";
  export let password = "";
  export let usernameError: string | undefined = undefined;
  export let passwordError: string | undefined = undefined;
  export let overviewMetrics: OverviewMetricViewModel[] = [];
  export let bleStatus: BleStatus | null = null;
  export let shellyDevices: ShellyDevice[] = [];
  export let matrixSettings: MatrixSettings | null = null;
  export let pendingShellyDeviceId: string | null = null;
  export let realtimeState: RealtimeConnectionState = "idle";
  export let showingCachedData = false;
  export let isSavingMatrixSettings = false;
  export let sectionVisibility: DeviceCardSectionState = {
    matrix: true,
    bluetooth: true,
    shelly: true,
  };
  export let onSelect: () => void | Promise<void> = () => undefined;
  export let onSignIn: () => void | Promise<void> = () => undefined;
  export let onOpenDevice: () => void | Promise<void> = () => undefined;
  export let onLogout: () => void | Promise<void> = () => undefined;
  export let onOpenSettings: () => void | Promise<void> = () => undefined;
  export let onReconnect: () => void | Promise<void> = () => undefined;
  export let onRename: (name: string) => boolean | Promise<boolean> = async () =>
    true;
  export let onSetShellyState: (
    shellyDeviceId: string,
    turnOn: boolean,
  ) => void | Promise<void> = () => undefined;
  export let onSaveMatrixSettings: (
    settings: Partial<MatrixSettings>,
    options?: { notify?: boolean },
  ) => void | Promise<void> = () => undefined;
  export let onSetSectionOpen: (
    section: DeviceCardSectionKey,
    open: boolean,
  ) => void | Promise<void> = () => undefined;
  export let onOpenCharts: (
    metricId: OverviewMetricId,
  ) => void | Promise<void> = () => undefined;
  export let onRemove: () => void | Promise<void> = () => undefined;

  $: bleThermometers = createBleThermometers(bleStatus);
</script>

<article class:selected={isSelected} class="device-card-shell">
  <DeviceCardHeader
    {device}
    {session}
    {isSelected}
    {realtimeState}
    {onSelect}
    {onOpenDevice}
    {onLogout}
    {onOpenSettings}
    {onReconnect}
    {onRename}
  />

  {#if isSelected}
    {#if !session}
      <DeviceCardAuthForm
        bind:username
        bind:password
        {usernameError}
        {passwordError}
        {isSigningIn}
        {onSignIn}
        {onOpenDevice}
        {onRemove}
      />
    {:else}
      <DeviceCardSignedInContent
        deviceId={device.id}
        {session}
        {overviewMetrics}
        {bleThermometers}
        {shellyDevices}
        {matrixSettings}
        {pendingShellyDeviceId}
        {showingCachedData}
        {isSavingMatrixSettings}
        bluetoothOpen={sectionVisibility.bluetooth}
        shellyOpen={sectionVisibility.shelly}
        {onSetShellyState}
        {onSaveMatrixSettings}
        onToggleBluetooth={() =>
          onSetSectionOpen("bluetooth", !sectionVisibility.bluetooth)}
        onToggleShelly={() => onSetSectionOpen("shelly", !sectionVisibility.shelly)}
        {onOpenCharts}
      />
    {/if}
  {/if}
</article>
