import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
import type { SidepanelControllerSessionRuntime } from "../sidepanelControllerSession";
import type { SidepanelControllerStore } from "../sidepanelControllerStore";
import type {
  OverviewSocketLike,
  SidepanelControllerDeps,
  SidepanelControllerOptions,
} from "../sidepanelControllerTypes";

export interface SelectedSessionContext {
  selectedDevice: DeviceRecord;
  currentSession: DeviceSession;
}

export interface SidepanelControllerActionContext {
  deps: SidepanelControllerDeps;
  store: SidepanelControllerStore;
  sessionRuntime: SidepanelControllerSessionRuntime;
  overviewSocket: OverviewSocketLike;
  onInfo: SidepanelControllerOptions["onInfo"];
  onError: SidepanelControllerOptions["onError"];
  clearMessages: SidepanelControllerOptions["clearMessages"];
}

export function getSelectedSessionContext(
  sessionRuntime: SidepanelControllerSessionRuntime,
): SelectedSessionContext | null {
  const { selectedDevice, currentSession } = sessionRuntime.getSelection();
  if (!selectedDevice || !currentSession) {
    return null;
  }

  return {
    selectedDevice,
    currentSession,
  };
}

export async function withSelectedSessionContext(
  context: SidepanelControllerActionContext,
  run: (selection: SelectedSessionContext) => Promise<void>,
): Promise<boolean> {
  const selection = getSelectedSessionContext(context.sessionRuntime);
  if (!selection) {
    return false;
  }

  await run(selection);
  return true;
}

export async function withAdminSessionContext(
  context: SidepanelControllerActionContext,
  permissionError: string,
  run: (selection: SelectedSessionContext) => Promise<void>,
): Promise<boolean> {
  const selection = getSelectedSessionContext(context.sessionRuntime);
  if (!selection) {
    return false;
  }

  if (!selection.currentSession.admin) {
    context.onError(permissionError);
    return false;
  }

  await run(selection);
  return true;
}
