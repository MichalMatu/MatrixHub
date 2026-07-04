import { createDeviceDraftErrors } from "$lib/features/devices/validation/deviceDraft";
import { renameDeviceRecord } from "$lib/domain/device/rules";
import { t } from "$lib/i18n/runtime";
import type { SidepanelControllerActionContext } from "./actionContext";

export function createDeviceActions(context: SidepanelControllerActionContext) {
  const { deps, store, sessionRuntime, onInfo, onError, clearMessages } =
    context;

  async function addDevice() {
    clearMessages();
    await store.commit("devices.add.started", (currentState) => {
      currentState.deviceFormErrors = createDeviceDraftErrors();
      currentState.activity.isSavingDevice = true;
    });

    try {
      const currentState = store.getState();
      const result = await deps.saveDeviceDraft({
        devices: currentState.devices,
        deviceName: currentState.deviceDraft.name,
        deviceAddress: currentState.deviceDraft.address,
      });

      if (result.kind === "permission_denied") {
        onError(result.message);
        return;
      }

      if (result.kind === "validation_error") {
        await store.commit("devices.add.validationFailed", (state) => {
          state.deviceFormErrors = result.errors;
        });
        onError(result.message);
        return;
      }

      const selectionChanged =
        result.selectedDeviceId !== currentState.selectedDeviceId;
      await store.commit(
        "devices.add.succeeded",
        (state) => {
          state.devices = result.devices;
          state.activeScreen = "devices";
          state.isAddSheetOpen = false;
          state.deviceDraft.name = "";
          state.deviceDraft.address = "";
        },
        {
          persist: selectionChanged ? "none" : "await",
        },
      );

      if (selectionChanged) {
        await sessionRuntime.applySelectedDeviceChange(result.selectedDeviceId);
      }

      onInfo(t("controller.deviceSaved"));
    } catch (error) {
      onError(error);
    } finally {
      await store.commit("devices.add.finished", (currentState) => {
        currentState.activity.isSavingDevice = false;
      });
    }
  }

  async function renameDevice(deviceId: string, name: string) {
    const currentState = store.getState();
    const trimmedName = name.trim();
    const currentDevice =
      currentState.devices.find((device) => device.id === deviceId) ?? null;

    if (!currentDevice) {
      return false;
    }

    if (!trimmedName) {
      onError(t("controller.deviceTitleEmpty"));
      return false;
    }

    if (currentDevice.name === trimmedName) {
      return true;
    }

    await store.commit(
      "devices.rename.saved",
      (state) => {
        state.devices = renameDeviceRecord(
          state.devices,
          deviceId,
          trimmedName,
        );
      },
      {
        persist: "await",
      },
    );
    onInfo(t("controller.deviceTitleSaved"));
    return true;
  }

  async function removeDevice(deviceId: string) {
    const currentState = store.getState();
    const nextContext = deps.buildRemoveDeviceContext({
      devices: currentState.devices,
      sessions: currentState.sessions,
      selectedDeviceId: currentState.selectedDeviceId,
      deviceId,
    });
    if (!nextContext) {
      return;
    }

    if (nextContext.hadSession) {
      await deps.clearWsAccessTokenCookie(nextContext.removedDevice.origin);
    }

    const selectionChanged =
      nextContext.selectedDeviceId !== currentState.selectedDeviceId;
    await store.commit(
      "devices.remove.completed",
      (state) => {
        state.devices = nextContext.devices;
        state.sessions = nextContext.sessions;
        store.removeDeviceSnapshot(deviceId);
        state.activeScreen = "devices";
        state.isAddSheetOpen = nextContext.devices.length === 0;
      },
      {
        persist: selectionChanged ? "none" : "await",
      },
    );

    if (selectionChanged) {
      await sessionRuntime.applySelectedDeviceChange(
        nextContext.selectedDeviceId,
      );
    }
  }

  return {
    addDevice,
    renameDevice,
    removeDevice,
  };
}
