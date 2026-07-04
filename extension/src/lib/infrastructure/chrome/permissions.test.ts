import { beforeEach, describe, expect, it, vi } from "vitest";

const browserMocks = vi.hoisted(() => ({
  contains: vi.fn(),
  request: vi.fn(),
}));

vi.mock("wxt/browser", () => ({
  browser: {
    permissions: {
      contains: browserMocks.contains,
      request: browserMocks.request,
    },
  },
}));

import { ensureDeviceOriginPermission } from "./permissions";

describe("device origin permissions", () => {
  beforeEach(() => {
    browserMocks.contains.mockReset();
    browserMocks.request.mockReset();
  });

  it("does not request host access when it is already granted", async () => {
    browserMocks.contains.mockResolvedValue(true);

    await expect(
      ensureDeviceOriginPermission("https://matrixhub.local"),
    ).resolves.toBe(true);
    expect(browserMocks.request).not.toHaveBeenCalled();
  });

  it("requests host access lazily for new origins", async () => {
    browserMocks.contains.mockResolvedValue(false);
    browserMocks.request.mockResolvedValue(true);

    await expect(
      ensureDeviceOriginPermission("https://matrixhub.local"),
    ).resolves.toBe(true);
    expect(browserMocks.request).toHaveBeenCalledWith({
      origins: ["https://matrixhub.local/*"],
    });
  });
});
