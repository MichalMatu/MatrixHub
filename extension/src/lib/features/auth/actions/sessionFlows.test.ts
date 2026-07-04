import { describe, expect, it, vi } from "vitest";
import { completeSignInFlow, restoreSelectedSessionFlow } from "./sessionFlows";

describe("session flows", () => {
  it("skips restore when no selected device or session exists", async () => {
    await expect(
      restoreSelectedSessionFlow({
        selectedDevice: null,
        currentSession: null,
        resetOverviewState: vi.fn(),
        hydrateRealtimeAuth: vi.fn(),
        refreshOverview: vi.fn(),
        connectRealtime: vi.fn(),
      }),
    ).resolves.toBe(false);
  });

  it("runs the complete sign-in flow in the expected order", async () => {
    const calls: string[] = [];

    await completeSignInFlow({
      afterSessionApplied: async () => {
        calls.push("afterSessionApplied");
      },
      hydrateRealtimeAuth: async () => {
        calls.push("hydrateRealtimeAuth");
      },
      persistState: async () => {
        calls.push("persistState");
      },
      refreshOverview: async () => {
        calls.push("refreshOverview");
      },
      connectRealtime: () => {
        calls.push("connectRealtime");
      },
      clearPassword: () => {
        calls.push("clearPassword");
      },
    });

    expect(calls).toEqual([
      "afterSessionApplied",
      "hydrateRealtimeAuth",
      "clearPassword",
      "persistState",
      "connectRealtime",
      "refreshOverview",
    ]);
  });

  it("continues realtime restore when overview refresh fails", async () => {
    const calls: string[] = [];
    const onRefreshOverviewError = vi.fn((error: unknown) => {
      calls.push(`onRefreshOverviewError:${String(error)}`);
    });

    await expect(
      restoreSelectedSessionFlow({
        selectedDevice: {
          id: "office",
          name: "Office",
          origin: "https://office.local",
          input: "office.local",
          createdAt: "2026-01-01T00:00:00.000Z",
        },
        currentSession: {
          accessToken: "token",
          username: "admin",
          admin: true,
          signedInAt: "2026-01-01T00:00:00.000Z",
        },
        resetOverviewState: () => {
          calls.push("resetOverviewState");
        },
        hydrateRealtimeAuth: async () => {
          calls.push("hydrateRealtimeAuth");
        },
        connectRealtime: () => {
          calls.push("connectRealtime");
        },
        refreshOverview: async () => {
          calls.push("refreshOverview");
          throw new Error("overview failed");
        },
        onRefreshOverviewError,
      }),
    ).resolves.toBe(true);

    expect(calls).toEqual([
      "resetOverviewState",
      "hydrateRealtimeAuth",
      "connectRealtime",
      "refreshOverview",
      "onRefreshOverviewError:Error: overview failed",
    ]);
    expect(onRefreshOverviewError).toHaveBeenCalledTimes(1);
  });
});
