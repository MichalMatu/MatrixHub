<script lang="ts">
  import { onDestroy, tick } from "svelte";
  import type { MatrixSettings } from "@matrixhub/device-sdk";
  import { browser } from "wxt/browser";
  import AddDeviceSheet from "$lib/features/sidepanel/ui/AddDeviceSheet.svelte";
  import DevicesScreen from "$lib/features/sidepanel/ui/screens/DevicesScreen.svelte";
  import EmptyStateScreen from "$lib/features/sidepanel/ui/screens/EmptyStateScreen.svelte";
  import SettingsScreen from "$lib/features/sidepanel/ui/screens/SettingsScreen.svelte";
  import TelemetryChartsPanel from "$lib/features/overview/ui/TelemetryChartsPanel.svelte";
  import SidepanelHeader from "$lib/features/sidepanel/ui/SidepanelHeader.svelte";
  import ToastRegion from "$lib/features/sidepanel/ui/ToastRegion.svelte";
  import { createDeviceDraftErrors } from "$lib/features/devices/validation/deviceDraft";
  import {
    buildDeviceSettingsViewModel,
    buildOverviewCardViewModel,
  } from "$lib/features/overview/mappers/viewModels";
  import { createSidepanelController } from "$lib/features/sidepanel/controller/sidepanelController";
  import {
    createInitialSidepanelState,
    getCurrentSession,
    getSelectedDevice,
    type SidepanelState,
  } from "$lib/features/sidepanel/state/sidepanelState";
  import type { DeviceCardSectionKey } from "$lib/features/sidepanel/state/deviceCardSections";
  import type { OverviewMetricId } from "$lib/features/overview/mappers/viewModels";
  import { describeAppError } from "$lib/domain/shared/appError";
  import { i18n } from "$lib/i18n/store";
  import type { LocalePreference } from "$lib/i18n/locales";
  import { setLocalePreference } from "$lib/i18n/runtime";
  import {
    setSidepanelTheme,
    type SidepanelThemeName,
  } from "$lib/features/theme/themePresets";
  import {
    createToastQueue,
    type ToastItem,
    type ToastTone,
  } from "$lib/features/toasts/toastQueue";

  let panelState: SidepanelState = createInitialSidepanelState();
  const MATRIX_QUICK_SAVE_TOAST_KEY = "matrix-led-quick-save";
  let activeChartMetricId: OverviewMetricId | null = null;

  let toasts: ToastItem[] = [];
  const toastQueue = createToastQueue({
    maxToasts: 3,
    lifetimeMs: 3000,
    onChange: (items) => {
      toasts = items;
    },
  });

  function showToast(tone: ToastTone, message: string) {
    toastQueue.show(tone, message);
  }

  function showInfo(message: string) {
    showToast("info", message);
  }

  function showError(error: unknown) {
    showToast(
      "error",
      typeof error === "string" ? error : describeAppError(error),
    );
  }

  function clearMessages() {
    toastQueue.clear();
  }

  const sidepanelController = createSidepanelController({
    state: panelState,
    onStateChange: (nextState) => {
      panelState = nextState;
    },
    onInfo: showInfo,
    onError: showError,
    clearMessages,
    deps: {
      afterSessionApplied: tick,
    },
  });

  $: selectedDevice = getSelectedDevice(panelState);
  $: currentSession = getCurrentSession(panelState);
  $: isShowingCachedTelemetry =
    panelState.selectedDeviceDataOrigins.telemetry === "cache";
  $: isShowingCachedCardData =
    isShowingCachedTelemetry ||
    panelState.selectedDeviceDataOrigins.ble === "cache" ||
    panelState.selectedDeviceDataOrigins.shelly === "cache";
  $: isShowingCachedSettingsData =
    isShowingCachedTelemetry ||
    panelState.selectedDeviceDataOrigins.overview === "cache";
  $: isShowingCachedMatrixSettings =
    panelState.selectedDeviceDataOrigins.matrix === "cache";
  $: overviewViewModel = buildOverviewCardViewModel({
    state: panelState.overviewState,
    i18n: $i18n,
  });
  $: settingsViewModel = buildDeviceSettingsViewModel({
    device: selectedDevice,
    session: currentSession,
    state: panelState.overviewState,
    realtimeState: panelState.realtimeState,
    isShowingCachedTelemetry,
    i18n: $i18n,
  });
  $: canManageMatrix = !!currentSession?.admin;

  // WXT's production build has been flaky about preserving this startup path
  // through onMount, so schedule bootstrap explicitly after component init.
  queueMicrotask(() => {
    void sidepanelController.bootstrap();
  });

  onDestroy(() => {
    sidepanelController.destroy();
  });

  async function addDevice() {
    await sidepanelController.addDevice();
  }

  async function signIn() {
    await sidepanelController.signIn();
  }

  async function reconnectDeviceCard(deviceId: string) {
    await sidepanelController.reconnectDevice(deviceId);
  }

  async function toggleShellyFromCard(
    deviceId: string,
    shellyDeviceId: string,
    turnOn: boolean,
  ) {
    await focusDevice(deviceId);
    await sidepanelController.toggleShelly(shellyDeviceId, turnOn);
  }

  async function openDeviceTab() {
    if (!selectedDevice) return;
    await browser.tabs.create({
      url: selectedDevice.origin,
    });
  }

  async function removeDevice(deviceId: string) {
    await sidepanelController.removeDevice(deviceId);
  }

  async function logout(message?: string) {
    await sidepanelController.logout(message);
  }

  async function refreshMatrixSettings() {
    await sidepanelController.refreshMatrixSettings();
  }

  async function saveMatrixSettings(
    settings: Partial<MatrixSettings>,
    options?: { notify?: boolean },
  ) {
    const isMatrixQuickSave = options?.notify === false;

    if (isMatrixQuickSave) {
      toastQueue.show("info", $i18n.t("matrix.quickSave.saving"), {
        key: MATRIX_QUICK_SAVE_TOAST_KEY,
      });
    }

    await sidepanelController.saveMatrixSettings(settings, options);

    if (!isMatrixQuickSave) {
      return;
    }

    if (panelState.matrixSettingsError) {
      toastQueue.dismissByKey(MATRIX_QUICK_SAVE_TOAST_KEY);
      return;
    }

    toastQueue.show("info", $i18n.t("matrix.quickSave.saved"), {
      key: MATRIX_QUICK_SAVE_TOAST_KEY,
    });
  }

  async function focusDevice(deviceId: string) {
    await sidepanelController.focusDevice(deviceId);
  }

  async function focusThen(
    deviceId: string,
    action: () => void | Promise<void>,
  ) {
    await focusDevice(deviceId);
    await action();
  }

  async function setSectionOpen(section: DeviceCardSectionKey, open: boolean) {
    await sidepanelController.setSectionOpen(section, open);
  }

  async function signInFromCard(deviceId: string) {
    await focusThen(deviceId, signIn);
  }

  async function openDeviceFromCard(deviceId: string) {
    await focusThen(deviceId, openDeviceTab);
  }

  async function logoutFromCard(deviceId: string) {
    await focusThen(deviceId, () => logout());
  }

  async function openSettingsForDevice(deviceId: string) {
    await focusThen(deviceId, () => {
      panelState.activeScreen = "settings";
    });
  }

  async function openChartsForDevice(
    deviceId: string,
    metricId: OverviewMetricId,
  ) {
    await focusThen(deviceId, () => {
      activeChartMetricId = metricId;
      panelState.activeScreen = "charts";
    });
  }

  async function renameDeviceFromCard(deviceId: string, name: string) {
    return sidepanelController.renameDevice(deviceId, name);
  }

  function openAddSheet() {
    panelState.deviceFormErrors = createDeviceDraftErrors();
    panelState.isAddSheetOpen = true;
  }

  function closeAddSheet() {
    if (panelState.activity.isSavingDevice) {
      return;
    }

    panelState.deviceFormErrors = createDeviceDraftErrors();
    panelState.isAddSheetOpen = false;
  }

  function returnToDevices() {
    panelState.activeScreen = "devices";
    activeChartMetricId = null;
  }

  function selectTheme(theme: SidepanelThemeName) {
    panelState.activeTheme = setSidepanelTheme(theme).name;
  }

  function selectLocale(nextPreference: LocalePreference) {
    setLocalePreference(nextPreference);
  }
</script>

<svelte:head>
  <title>{$i18n.t("app.title")}</title>
  <link href="/icon-32.png" rel="icon" />
</svelte:head>

<SidepanelHeader
  activeScreen={panelState.activeScreen}
  selectedDeviceName={selectedDevice?.name ?? null}
  activeTheme={panelState.activeTheme}
  localePreference={$i18n.preference}
  onBack={returnToDevices}
  onOpenAddSheet={openAddSheet}
  onSelectTheme={selectTheme}
  onSelectLocale={selectLocale}
/>

<div class="shell">
  {#if panelState.activeScreen === "devices" && panelState.devices.length > 0}
    <DevicesScreen
      state={panelState}
      overviewMetrics={overviewViewModel.metrics}
      showingCachedData={isShowingCachedCardData}
      onFocusDevice={focusDevice}
      onSignInDevice={signInFromCard}
      onOpenDevice={openDeviceFromCard}
      onLogoutDevice={logoutFromCard}
      onOpenSettings={openSettingsForDevice}
      onReconnectDevice={reconnectDeviceCard}
      onRenameDevice={renameDeviceFromCard}
      onToggleShelly={toggleShellyFromCard}
      onSaveMatrixSettings={saveMatrixSettings}
      onSetSectionOpen={setSectionOpen}
      onOpenCharts={openChartsForDevice}
      onRemoveDevice={removeDevice}
    />
  {/if}

  {#if panelState.activeScreen === "settings"}
    <SettingsScreen
      selectedDeviceName={selectedDevice?.name ?? null}
      {settingsViewModel}
      showingCachedSettingsData={isShowingCachedSettingsData}
      canSignOut={!!currentSession}
      matrixSettings={panelState.matrixSettings}
      isLoadingMatrixSettings={panelState.activity.isLoadingMatrixSettings}
      isSavingMatrixSettings={panelState.activity.isSavingMatrixSettings}
      {canManageMatrix}
      showingCachedMatrixSettings={isShowingCachedMatrixSettings}
      matrixSettingsError={panelState.matrixSettingsError}
      onOpenDevice={openDeviceTab}
      onLogout={() => logout()}
      onRefreshMatrixSettings={refreshMatrixSettings}
      onSaveMatrixSettings={saveMatrixSettings}
    />
  {/if}

  {#if panelState.activeScreen === "charts"}
    <div class="settings-page">
      <TelemetryChartsPanel
        metrics={overviewViewModel.metrics}
        selectedMetricId={activeChartMetricId}
        showingCachedData={isShowingCachedTelemetry}
      />
    </div>
  {/if}

  {#if panelState.activeScreen === "devices" && panelState.devices.length === 0}
    <EmptyStateScreen onOpenAddSheet={openAddSheet} />
  {/if}
</div>

<AddDeviceSheet
  isOpen={panelState.isAddSheetOpen}
  bind:deviceName={panelState.deviceDraft.name}
  bind:deviceAddress={panelState.deviceDraft.address}
  addressError={panelState.deviceFormErrors.address}
  isSavingDevice={panelState.activity.isSavingDevice}
  onAddDevice={addDevice}
  onClose={closeAddSheet}
/>

<ToastRegion isBootstrapping={panelState.isBootstrapping} {toasts} />
