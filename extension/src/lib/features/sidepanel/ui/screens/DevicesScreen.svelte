<script lang="ts">
  import type { MatrixSettings } from "@matrixhub/device-sdk";
  import DeviceCard from "$lib/features/devices/ui/DeviceCard.svelte";
  import type {
    OverviewMetricId,
    OverviewMetricViewModel,
  } from "$lib/features/overview/mappers/viewModels";
  import type { DeviceCardSectionKey } from "$lib/features/sidepanel/state/deviceCardSections";
  import type { SidepanelState } from "$lib/features/sidepanel/state/sidepanelState";

  export let state: SidepanelState;
  export let overviewMetrics: OverviewMetricViewModel[] = [];
  export let showingCachedData = false;
  export let onFocusDevice: (deviceId: string) => void | Promise<void> = () =>
    undefined;
  export let onSignInDevice: (deviceId: string) => void | Promise<void> = () =>
    undefined;
  export let onOpenDevice: (deviceId: string) => void | Promise<void> = () =>
    undefined;
  export let onLogoutDevice: (deviceId: string) => void | Promise<void> = () =>
    undefined;
  export let onOpenSettings: (deviceId: string) => void | Promise<void> = () =>
    undefined;
  export let onReconnectDevice: (deviceId: string) => void | Promise<void> = () =>
    undefined;
  export let onRenameDevice: (
    deviceId: string,
    name: string,
  ) => boolean | Promise<boolean> = async () => true;
  export let onToggleShelly: (
    deviceId: string,
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
    deviceId: string,
    metricId: OverviewMetricId,
  ) => void | Promise<void> = () => undefined;
  export let onRemoveDevice: (deviceId: string) => void | Promise<void> = () =>
    undefined;
</script>

<section class="device-deck">
  {#each state.devices as device (device.id)}
    <DeviceCard
      {device}
      session={state.sessions[device.id] ?? null}
      isSelected={device.id === state.selectedDeviceId}
      bind:username={state.credentials.username}
      bind:password={state.credentials.password}
      usernameError={device.id === state.selectedDeviceId
        ? state.authFormErrors.username
        : undefined}
      passwordError={device.id === state.selectedDeviceId
        ? state.authFormErrors.password
        : undefined}
      isSigningIn={device.id === state.selectedDeviceId &&
        state.activity.isSigningIn}
      overviewMetrics={device.id === state.selectedDeviceId
        ? overviewMetrics
        : []}
      bleStatus={device.id === state.selectedDeviceId ? state.bleStatus : null}
      shellyDevices={device.id === state.selectedDeviceId
        ? state.shellyDevices
        : []}
      matrixSettings={device.id === state.selectedDeviceId
        ? state.matrixSettings
        : null}
      pendingShellyDeviceId={device.id === state.selectedDeviceId
        ? state.activity.pendingShellyDeviceId
        : null}
      realtimeState={device.id === state.selectedDeviceId
        ? state.realtimeState
        : "idle"}
      showingCachedData={device.id === state.selectedDeviceId
        ? showingCachedData
        : false}
      isSavingMatrixSettings={device.id === state.selectedDeviceId &&
        state.activity.isSavingMatrixSettings}
      sectionVisibility={state.sectionVisibility}
      onSelect={() => onFocusDevice(device.id)}
      onSignIn={() => onSignInDevice(device.id)}
      onOpenDevice={() => onOpenDevice(device.id)}
      onLogout={() => onLogoutDevice(device.id)}
      onOpenSettings={() => onOpenSettings(device.id)}
      onReconnect={() => onReconnectDevice(device.id)}
      onRename={(name) => onRenameDevice(device.id, name)}
      onSetShellyState={(shellyDeviceId, turnOn) =>
        onToggleShelly(device.id, shellyDeviceId, turnOn)}
      {onSaveMatrixSettings}
      {onSetSectionOpen}
      onOpenCharts={(metricId) => onOpenCharts(device.id, metricId)}
      onRemove={() => onRemoveDevice(device.id)}
    />
  {/each}
</section>
