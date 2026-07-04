import type { SidepanelControllerDeps } from "./sidepanelControllerTypes";
import { DeviceOverviewSocket } from "$lib/features/realtime/socket/deviceOverviewSocket";
import {
  clearWsAccessTokenCookie,
  syncWsAccessTokenCookie,
} from "$lib/infrastructure/chrome/cookies";
import {
  loadExtensionState,
  saveExtensionState,
} from "$lib/infrastructure/chrome/storage";
import {
  createBleApiForSelection,
  createMatrixApiForSelection,
  createShellyApiForSelection,
  createSystemApiForSelection,
} from "$lib/infrastructure/device/apiFactory";
import { signInSelectedDevice } from "$lib/features/auth/actions/signIn";
import {
  completeSignInFlow,
  restoreSelectedSessionFlow,
} from "$lib/features/auth/actions/sessionFlows";
import { saveDeviceDraft } from "$lib/features/devices/actions/deviceDraft";
import {
  buildRemoveDeviceContext,
  clearSelectedSessionContext,
} from "$lib/features/devices/actions/deviceSelection";
import {
  fetchOverviewSnapshot,
  requestWifiRecoveryMessage,
} from "$lib/features/overview/actions/overviewData";
import { markDeviceConnected } from "$lib/domain/device/rules";

export const DEFAULT_DEPS: SidepanelControllerDeps = {
  afterSessionApplied: async () => undefined,
  loadExtensionState,
  saveExtensionState,
  syncWsAccessTokenCookie,
  clearWsAccessTokenCookie,
  saveDeviceDraft,
  signInSelectedDevice,
  restoreSelectedSessionFlow,
  completeSignInFlow,
  createSystemApiForSelection,
  createBleApiForSelection,
  createShellyApiForSelection,
  createMatrixApiForSelection,
  fetchOverviewSnapshot: (api) => fetchOverviewSnapshot(api as never),
  requestWifiRecoveryMessage: (api) => requestWifiRecoveryMessage(api as never),
  buildRemoveDeviceContext,
  clearSelectedSessionContext,
  markDeviceConnected,
  createOverviewSocket: (handlers) => new DeviceOverviewSocket(handlers),
};
