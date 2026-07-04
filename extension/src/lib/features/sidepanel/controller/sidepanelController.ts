import { DEFAULT_DEPS } from "./sidepanelControllerDeps";
import { createSidepanelOverviewSocket } from "./sidepanelControllerRealtime";
import { createSidepanelControllerSessionRuntime } from "./sidepanelControllerSession";
import { createSidepanelControllerStore } from "./sidepanelControllerStore";
import { createBootstrapActions } from "./actions/bootstrapActions";
import { createDeviceActions } from "./actions/deviceActions";
import { createSelectionActions } from "./actions/selectionActions";
import { createSessionActions } from "./actions/sessionActions";
import type {
  SidepanelController,
  SidepanelControllerOptions,
} from "./sidepanelControllerTypes";

export function createSidepanelController(
  options: SidepanelControllerOptions,
): SidepanelController {
  const deps = {
    ...DEFAULT_DEPS,
    ...options.deps,
  };

  const store = createSidepanelControllerStore({
    initialState: options.state,
    onStateChange: options.onStateChange,
    onError: options.onError,
    saveExtensionState: deps.saveExtensionState,
    onTransition: options.onTransition,
  });

  const logoutRef: { current: SidepanelController["logout"] } = {
    current: async () => undefined,
  };

  const overviewSocket = createSidepanelOverviewSocket({
    createOverviewSocket: deps.createOverviewSocket,
    store,
    onError: options.onError,
  });

  const sessionRuntime = createSidepanelControllerSessionRuntime({
    deps,
    store,
    overviewSocket,
    onInfo: options.onInfo,
    onError: options.onError,
    clearMessages: options.clearMessages,
    logout: (message) => logoutRef.current(message),
  });

  const actionContext = {
    deps,
    store,
    sessionRuntime,
    overviewSocket,
    onInfo: options.onInfo,
    onError: options.onError,
    clearMessages: options.clearMessages,
  };

  const { bootstrap } = createBootstrapActions(actionContext);
  const { addDevice, renameDevice, removeDevice } =
    createDeviceActions(actionContext);
  const sessionActions = createSessionActions(actionContext);
  const selectionActions = createSelectionActions(actionContext, {
    triggerWifiRecovery: sessionActions.triggerWifiRecovery,
  });

  const controller: SidepanelController = {
    bootstrap,
    addDevice,
    signIn: sessionActions.signIn,
    refreshOverview: sessionActions.refreshOverview,
    refreshMatrixSettings: sessionActions.refreshMatrixSettings,
    saveMatrixSettings: sessionActions.saveMatrixSettings,
    toggleShelly: sessionActions.toggleShelly,
    triggerWifiRecovery: sessionActions.triggerWifiRecovery,
    selectDevice: selectionActions.selectDevice,
    setSectionOpen: selectionActions.setSectionOpen,
    focusDevice: selectionActions.focusDevice,
    reconnectDevice: selectionActions.reconnectDevice,
    renameDevice,
    removeDevice,
    logout: sessionActions.logout,
    destroy() {
      overviewSocket.disconnect();
      options.clearMessages();
    },
  };

  logoutRef.current = controller.logout;

  return controller;
}

export type {
  SidepanelController,
  SidepanelControllerDeps,
  SidepanelControllerOptions,
} from "./sidepanelControllerTypes";
