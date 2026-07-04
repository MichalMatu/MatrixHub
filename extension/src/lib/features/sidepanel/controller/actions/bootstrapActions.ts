import type { SidepanelControllerActionContext } from "./actionContext";

export function createBootstrapActions(
  context: SidepanelControllerActionContext,
) {
  const { deps, store, sessionRuntime, onError } = context;

  async function bootstrap() {
    try {
      const state = await deps.loadExtensionState();
      await store.commit("bootstrap.loaded", (currentState) => {
        currentState.devices = state.devices;
        currentState.selectedDeviceId =
          state.selectedDeviceId ?? state.devices[0]?.id ?? null;
        currentState.sessions = state.sessions;
        currentState.deviceSnapshots = state.deviceSnapshots;
        currentState.sectionVisibility = state.sectionVisibility;
        store.restoreSelectedDeviceSnapshot(currentState.selectedDeviceId);
        currentState.activeScreen = "devices";
        currentState.isAddSheetOpen = state.devices.length === 0;
      });

      void sessionRuntime.restoreSelectedSessionIfAvailable();
    } catch (error) {
      onError(error);
    } finally {
      await store.commit("bootstrap.finished", (currentState) => {
        currentState.isBootstrapping = false;
      });
    }
  }

  return {
    bootstrap,
  };
}
