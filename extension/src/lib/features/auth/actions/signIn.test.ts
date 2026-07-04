import { beforeEach, describe, expect, it, vi } from "vitest";

const sdkMocks = vi.hoisted(() => ({
  decodeAccessTokenPayload: vi.fn(),
  signInToDevice: vi.fn(),
}));

const permissionMocks = vi.hoisted(() => ({
  ensureDeviceOriginPermission: vi.fn(),
}));

vi.mock("@matrixhub/device-sdk", async () => {
  const actual = await vi.importActual<typeof import("@matrixhub/device-sdk")>(
    "@matrixhub/device-sdk",
  );

  return {
    ...actual,
    decodeAccessTokenPayload: sdkMocks.decodeAccessTokenPayload,
    signInToDevice: sdkMocks.signInToDevice,
  };
});

vi.mock("$lib/infrastructure/chrome/permissions", () => ({
  ensureDeviceOriginPermission: permissionMocks.ensureDeviceOriginPermission,
}));

import { signInSelectedDevice } from "./signIn";

const selectedDevice = {
  id: "office",
  name: "Office",
  origin: "https://matrixhub.local",
  input: "matrixhub.local",
  createdAt: "2026-01-01T00:00:00.000Z",
};

describe("signInSelectedDevice", () => {
  beforeEach(() => {
    sdkMocks.decodeAccessTokenPayload.mockReset();
    sdkMocks.signInToDevice.mockReset();
    permissionMocks.ensureDeviceOriginPermission.mockReset();
  });

  it("returns validation errors before requesting permissions", async () => {
    const result = await signInSelectedDevice({
      selectedDevice,
      username: "",
      password: "",
    });

    expect(result.kind).toBe("validation_error");
    expect(permissionMocks.ensureDeviceOriginPermission).not.toHaveBeenCalled();
  });

  it("returns permission_denied when host access is rejected", async () => {
    permissionMocks.ensureDeviceOriginPermission.mockResolvedValue(false);

    await expect(
      signInSelectedDevice({
        selectedDevice,
        username: "admin",
        password: "secret",
      }),
    ).resolves.toEqual({
      kind: "permission_denied",
      message: "Host permission was not granted for this device.",
    });
  });

  it("creates a session from the decoded access token payload", async () => {
    permissionMocks.ensureDeviceOriginPermission.mockResolvedValue(true);
    sdkMocks.signInToDevice.mockResolvedValue({
      access_token: "token",
    });
    sdkMocks.decodeAccessTokenPayload.mockReturnValue({
      username: "admin",
      admin: true,
    });

    const result = await signInSelectedDevice({
      selectedDevice,
      username: "admin",
      password: "secret",
    });

    expect(result.kind).toBe("success");
    if (result.kind !== "success") {
      return;
    }

    expect(result.session).toMatchObject({
      accessToken: "token",
      username: "admin",
      admin: true,
    });
  });
});
