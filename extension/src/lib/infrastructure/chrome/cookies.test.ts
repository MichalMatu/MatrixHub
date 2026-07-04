import { beforeEach, describe, expect, it, vi } from "vitest";

const browserMocks = vi.hoisted(() => ({
  set: vi.fn(),
  remove: vi.fn(),
}));

vi.mock("wxt/browser", () => ({
  browser: {
    cookies: {
      set: browserMocks.set,
      remove: browserMocks.remove,
    },
  },
}));

import { clearWsAccessTokenCookie, syncWsAccessTokenCookie } from "./cookies";

describe("websocket auth cookies", () => {
  beforeEach(() => {
    browserMocks.set.mockReset();
    browserMocks.remove.mockReset();
  });

  it("writes a cross-site secure cookie for https origins", async () => {
    await syncWsAccessTokenCookie("https://matrixhub.local", "token");

    expect(browserMocks.set).toHaveBeenCalledWith({
      url: "https://matrixhub.local/ws",
      name: "access_token",
      value: "token",
      path: "/ws",
      secure: true,
      sameSite: "no_restriction",
    });
  });

  it("clears both websocket cookie paths", async () => {
    await clearWsAccessTokenCookie("https://matrixhub.local");

    expect(browserMocks.remove).toHaveBeenNthCalledWith(1, {
      url: "https://matrixhub.local/ws",
      name: "access_token",
    });
    expect(browserMocks.remove).toHaveBeenNthCalledWith(2, {
      url: "https://matrixhub.local/",
      name: "access_token",
    });
  });
});
