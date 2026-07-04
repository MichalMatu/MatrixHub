import type { DeviceSystemApi } from "@matrixhub/device-sdk";
import { t } from "$lib/i18n/runtime";

export async function fetchOverviewSnapshot(api: DeviceSystemApi) {
  const systemInfo = await api.getSystemInfo();

  return {
    connectedAt: new Date().toISOString(),
    systemInfo,
  };
}

export async function requestWifiRecoveryMessage(api: DeviceSystemApi) {
  const response = await api.triggerWifiRecovery();

  return response.connected
    ? t("overviewActions.reconnectRequestedWithAddress", {
        address: response.ip ?? t("overviewActions.currentAddress"),
      })
    : t("overviewActions.reconnectRequestedAccepted");
}
