import { browser } from "wxt/browser";
import { buildDevicePermissionOrigin } from "@matrixhub/device-sdk";

export async function ensureDeviceOriginPermission(origin: string) {
  // Host permissions are requested per device origin on demand. This keeps the
  // extension least-privileged and also makes it obvious during debugging why a
  // request fails before login or restoreSession runs.
  const permission = {
    origins: [buildDevicePermissionOrigin(origin)],
  };

  const alreadyGranted = await browser.permissions.contains(permission);
  if (alreadyGranted) {
    return true;
  }

  return browser.permissions.request(permission);
}
