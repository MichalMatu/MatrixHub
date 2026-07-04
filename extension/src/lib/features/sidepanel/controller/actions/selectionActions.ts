import type { DeviceCardSectionKey } from "$lib/features/sidepanel/state/deviceCardSections";
import type { SidepanelControllerActionContext } from "./actionContext";

interface SelectionActionsDeps {
  triggerWifiRecovery: () => Promise<void>;
}

export function createSelectionActions(
  context: SidepanelControllerActionContext,
  deps: SelectionActionsDeps,
) {
  const { store, sessionRuntime } = context;

  async function selectDevice(deviceId: string) {
    await sessionRuntime.applySelectedDeviceChange(deviceId);
  }

  async function setSectionOpen(section: DeviceCardSectionKey, open: boolean) {
    const currentState = store.getState();
    if (currentState.sectionVisibility[section] === open) {
      return;
    }

    await store.commit(
      "ui.sectionVisibility.changed",
      (state) => {
        state.sectionVisibility = {
          ...state.sectionVisibility,
          [section]: open,
        };
      },
      {
        persist: "background",
      },
    );
  }

  async function focusDevice(deviceId: string) {
    if (deviceId !== store.getState().selectedDeviceId) {
      await selectDevice(deviceId);
    }

    await store.commit("selection.focused", (currentState) => {
      currentState.activeScreen = "devices";
    });
  }

  async function reconnectDevice(deviceId: string) {
    await focusDevice(deviceId);
    await deps.triggerWifiRecovery();
  }

  return {
    selectDevice,
    setSectionOpen,
    focusDevice,
    reconnectDevice,
  };
}
