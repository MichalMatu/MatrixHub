import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";

interface RestoreSelectedSessionFlowInput {
  selectedDevice: DeviceRecord | null;
  currentSession: DeviceSession | null;
  resetOverviewState: () => void;
  hydrateRealtimeAuth: () => Promise<void>;
  refreshOverview: () => Promise<void>;
  connectRealtime: () => void;
  onRefreshOverviewError?: (error: unknown) => void;
}

export async function restoreSelectedSessionFlow(
  input: RestoreSelectedSessionFlowInput,
) {
  if (!input.selectedDevice || !input.currentSession) {
    return false;
  }

  input.resetOverviewState();
  await input.hydrateRealtimeAuth();
  input.connectRealtime();
  try {
    await input.refreshOverview();
  } catch (error) {
    input.onRefreshOverviewError?.(error);
  }

  return true;
}

interface CompleteSignInFlowInput {
  afterSessionApplied: () => Promise<void>;
  hydrateRealtimeAuth: () => Promise<void>;
  persistState: () => Promise<void>;
  refreshOverview: () => Promise<void>;
  connectRealtime: () => void;
  clearPassword: () => void;
  onRefreshOverviewError?: (error: unknown) => void;
}

export async function completeSignInFlow(input: CompleteSignInFlowInput) {
  await input.afterSessionApplied();
  await input.hydrateRealtimeAuth();
  input.clearPassword();
  await input.persistState();
  input.connectRealtime();
  try {
    await input.refreshOverview();
  } catch (error) {
    input.onRefreshOverviewError?.(error);
  }
}
