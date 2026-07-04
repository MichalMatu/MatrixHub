import {
  createDeviceRecord,
  normalizeDeviceOrigin,
  suggestDeviceName,
  type DeviceRecord,
} from "@matrixhub/device-sdk";
import { createAppError } from "$lib/domain/shared/appError";
import { upsertDeviceRecord } from "$lib/domain/device/rules";
import { ensureDeviceOriginPermission } from "$lib/infrastructure/chrome/permissions";
import {
  validateDeviceDraft,
  type DeviceDraftErrors,
} from "$lib/features/devices/validation/deviceDraft";

interface SaveDeviceDraftInput {
  devices: DeviceRecord[];
  deviceName: string;
  deviceAddress: string;
}

type SaveDeviceDraftResult =
  | {
      kind: "success";
      devices: DeviceRecord[];
      selectedDeviceId: string;
    }
  | {
      kind: "validation_error";
      errors: DeviceDraftErrors;
      message: string;
    }
  | {
      kind: "permission_denied";
      message: string;
    };

export async function saveDeviceDraft(
  input: SaveDeviceDraftInput,
): Promise<SaveDeviceDraftResult> {
  const validation = validateDeviceDraft({
    name: input.deviceName,
    address: input.deviceAddress,
  });

  if (!validation.ok) {
    return {
      kind: "validation_error",
      errors: validation.error,
      message:
        validation.error.address ?? createAppError("request/failed").message,
    };
  }

  const normalized = normalizeDeviceOrigin(validation.value.address);
  const allowed = await ensureDeviceOriginPermission(normalized.origin);
  if (!allowed) {
    return {
      kind: "permission_denied",
      message: createAppError("permissions/host_denied").message,
    };
  }

  const existing =
    input.devices.find((device) => device.origin === normalized.origin) ?? null;
  const nextDevice = createDeviceRecord({
    origin: normalized.origin,
    rawInput: validation.value.address,
    name: validation.value.name || suggestDeviceName(normalized.origin),
    existing,
  });

  return {
    kind: "success",
    devices: upsertDeviceRecord(input.devices, nextDevice),
    selectedDeviceId: nextDevice.id,
  };
}
