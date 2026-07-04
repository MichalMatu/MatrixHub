import { beforeEach, describe, expect, it, vi } from "vitest";

const permissionMocks = vi.hoisted(() => ({
  ensureDeviceOriginPermission: vi.fn(),
}));

vi.mock("$lib/infrastructure/chrome/permissions", () => ({
  ensureDeviceOriginPermission: permissionMocks.ensureDeviceOriginPermission,
}));

import { saveDeviceDraft } from "./deviceDraft";

describe("saveDeviceDraft", () => {
  beforeEach(() => {
    permissionMocks.ensureDeviceOriginPermission.mockReset();
  });

  it("returns validation errors for invalid device addresses", async () => {
    const result = await saveDeviceDraft({
      devices: [],
      deviceName: "Office",
      deviceAddress: "not a url with spaces",
    });

    expect(result.kind).toBe("validation_error");
  });

  it("returns permission_denied when origin access is rejected", async () => {
    permissionMocks.ensureDeviceOriginPermission.mockResolvedValue(false);

    await expect(
      saveDeviceDraft({
        devices: [],
        deviceName: "Office",
        deviceAddress: "matrixhub.local",
      }),
    ).resolves.toEqual({
      kind: "permission_denied",
      message: "Host permission was not granted for this device.",
    });
  });

  it("normalizes and upserts device records on success", async () => {
    permissionMocks.ensureDeviceOriginPermission.mockResolvedValue(true);

    const result = await saveDeviceDraft({
      devices: [],
      deviceName: "",
      deviceAddress: "matrixhub.local",
    });

    expect(result.kind).toBe("success");
    if (result.kind !== "success") {
      return;
    }

    expect(result.selectedDeviceId).toBe("https-matrixhub-local");
    expect(result.devices[0]).toMatchObject({
      name: "matrixhub",
      origin: "https://matrixhub.local",
    });
  });
});
