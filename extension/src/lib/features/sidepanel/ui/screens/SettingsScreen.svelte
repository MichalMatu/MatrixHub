<script lang="ts">
  import type { MatrixSettings } from "@matrixhub/device-sdk";
  import type { DeviceSettingsViewModel } from "$lib/features/overview/mappers/viewModels";
  import DeviceSettingsPanel from "$lib/features/overview/ui/DeviceSettingsPanel.svelte";
  import MatrixSettingsPanel from "$lib/features/matrix/ui/MatrixSettingsPanel.svelte";

  export let selectedDeviceName: string | null = null;
  export let settingsViewModel: DeviceSettingsViewModel;
  export let showingCachedSettingsData = false;
  export let canSignOut = false;
  export let matrixSettings: MatrixSettings | null = null;
  export let isLoadingMatrixSettings = false;
  export let isSavingMatrixSettings = false;
  export let canManageMatrix = false;
  export let showingCachedMatrixSettings = false;
  export let matrixSettingsError: string | null = null;
  export let onOpenDevice: () => void | Promise<void> = () => undefined;
  export let onLogout: () => void | Promise<void> = () => undefined;
  export let onRefreshMatrixSettings: () => void | Promise<void> = () =>
    undefined;
  export let onSaveMatrixSettings: (
    settings: Partial<MatrixSettings>,
    options?: { notify?: boolean },
  ) => void | Promise<void> = () => undefined;
</script>

<div class="settings-page">
  <DeviceSettingsPanel
    heading={selectedDeviceName ?? ""}
    viewModel={settingsViewModel}
    showingCachedData={showingCachedSettingsData}
    {canSignOut}
    {onOpenDevice}
    {onLogout}
  />

  <MatrixSettingsPanel
    settings={matrixSettings}
    loading={isLoadingMatrixSettings}
    saving={isSavingMatrixSettings}
    canManage={canManageMatrix}
    showingCachedData={showingCachedMatrixSettings}
    errorMessage={matrixSettingsError}
    onReload={onRefreshMatrixSettings}
    onSave={onSaveMatrixSettings}
  />
</div>
