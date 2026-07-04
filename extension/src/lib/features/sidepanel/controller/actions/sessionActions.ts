import { createCredentialErrors } from "$lib/features/auth/validation/credentials";
import { t } from "$lib/i18n/runtime";
import type { MatrixSettings } from "@matrixhub/device-sdk";
import type { MatrixSettingsSaveOptions } from "../sidepanelControllerTypes";
import type { SidepanelControllerActionContext } from "./actionContext";
import {
  withAdminSessionContext,
  withSelectedSessionContext,
} from "./actionContext";

export function createSessionActions(
  context: SidepanelControllerActionContext,
) {
  const {
    deps,
    store,
    sessionRuntime,
    overviewSocket,
    onInfo,
    onError,
    clearMessages,
  } = context;

  async function signIn() {
    const { selectedDevice } = sessionRuntime.getSelection();
    if (!selectedDevice) {
      onError(t("controller.addDeviceFirst"));
      return;
    }

    clearMessages();
    await store.commit("auth.signIn.started", (currentState) => {
      currentState.authFormErrors = createCredentialErrors();
      currentState.activity.isSigningIn = true;
    });

    try {
      const currentState = store.getState();
      const result = await deps.signInSelectedDevice({
        selectedDevice,
        username: currentState.credentials.username,
        password: currentState.credentials.password,
      });

      if (result.kind === "permission_denied") {
        onError(result.message);
        return;
      }

      if (result.kind === "validation_error") {
        await store.commit("auth.signIn.validationFailed", (state) => {
          state.authFormErrors = result.errors;
        });
        onError(result.message);
        return;
      }

      if (result.kind === "invalid_credentials") {
        onError(result.message);
        return;
      }

      await store.commit("auth.signIn.sessionStored", (state) => {
        state.sessions = {
          ...state.sessions,
          [selectedDevice.id]: result.session,
        };
      });

      await sessionRuntime.completeSignIn(
        selectedDevice,
        result.session,
        () => {
          store.getState().credentials.password = "";
        },
      );

      if (store.getState().selectedDeviceId === selectedDevice.id) {
        await store.commit("auth.signIn.screenFocused", (currentState) => {
          currentState.activeScreen = "devices";
        });
      }

      onInfo(t("controller.signedIn"));
    } catch (error) {
      onError(error);
    } finally {
      await store.commit("auth.signIn.finished", (currentState) => {
        currentState.activity.isSigningIn = false;
      });
    }
  }

  async function refreshOverview() {
    await withSelectedSessionContext(
      context,
      async ({ selectedDevice, currentSession }) => {
        await sessionRuntime.refreshOverviewFor(selectedDevice, currentSession);
      },
    );
  }

  async function refreshMatrixSettings() {
    await withSelectedSessionContext(
      context,
      async ({ selectedDevice, currentSession }) => {
        try {
          await sessionRuntime.refreshMatrixSettingsFor(
            selectedDevice,
            currentSession,
          );
        } catch (error) {
          onError(error);
        }
      },
    );
  }

  async function saveMatrixSettings(
    settings: Partial<MatrixSettings>,
    saveOptions?: MatrixSettingsSaveOptions,
  ) {
    await withAdminSessionContext(
      context,
      t("controller.matrixAdminRequired"),
      async ({ selectedDevice, currentSession }) => {
        if (saveOptions?.notify !== false) {
          clearMessages();
        }

        try {
          await sessionRuntime.saveMatrixSettingsFor(
            selectedDevice,
            currentSession,
            settings,
            saveOptions,
          );
        } catch (error) {
          onError(error);
        }
      },
    );
  }

  async function toggleShelly(shellyDeviceId: string, turnOn: boolean) {
    await withAdminSessionContext(
      context,
      t("controller.shellyAdminRequired"),
      async ({ selectedDevice, currentSession }) => {
        try {
          await sessionRuntime.toggleShellyFor(
            selectedDevice,
            currentSession,
            shellyDeviceId,
            turnOn,
          );
        } catch (error) {
          onError(error);
        }
      },
    );
  }

  async function triggerWifiRecovery() {
    await withSelectedSessionContext(
      context,
      async ({ selectedDevice, currentSession }) => {
        try {
          await sessionRuntime.triggerWifiRecoveryFor(
            selectedDevice,
            currentSession,
          );
        } catch (error) {
          onError(error);
        }
      },
    );
  }

  async function logout(message = t("controller.signedOut")) {
    const { selectedDevice } = sessionRuntime.getSelection();
    if (selectedDevice) {
      await deps.clearWsAccessTokenCookie(selectedDevice.origin);
    }

    overviewSocket.disconnect();
    await store.commit(
      "auth.logout.completed",
      (currentState) => {
        currentState.sessions = deps.clearSelectedSessionContext(
          currentState.sessions,
          currentState.selectedDeviceId,
        );
        currentState.authFormErrors = createCredentialErrors();
        currentState.credentials.password = "";
        currentState.activeScreen = "devices";
        store.resetSelectionState();
      },
      {
        persist: "await",
      },
    );
    onInfo(message);
  }

  return {
    signIn,
    refreshOverview,
    refreshMatrixSettings,
    saveMatrixSettings,
    toggleShelly,
    triggerWifiRecovery,
    logout,
  };
}
