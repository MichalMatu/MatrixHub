import { normalizeDeviceOrigin } from "@matrixhub/device-sdk";
import { createAppError } from "$lib/domain/shared/appError";
import { err, ok, type Result } from "$lib/domain/shared/result";

export interface DeviceDraftValues {
  name: string;
  address: string;
}

export interface DeviceDraftErrors {
  address?: string;
}

export function createDeviceDraftErrors(): DeviceDraftErrors {
  return {};
}

export function validateDeviceDraft(
  values: DeviceDraftValues,
): Result<DeviceDraftValues, DeviceDraftErrors> {
  const normalized = {
    name: values.name.trim(),
    address: values.address.trim(),
  };

  if (!normalized.address) {
    return err({
      address: createAppError("validation/device_address_required").message,
    });
  }

  try {
    normalizeDeviceOrigin(normalized.address);
  } catch {
    return err({
      address: createAppError("validation/device_address_invalid").message,
    });
  }

  return ok(normalized);
}
