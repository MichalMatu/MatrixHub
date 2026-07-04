import { describe, expect, it, vi } from "vitest";

import { buildDeviceApiUrl, createDeviceApiClient } from "./api/client";
import { ApiError, getRequestFailureKind } from "./api/errors";
import { buildCsiWebSocketUrl } from "./wifisensing";
import { buildDeviceWebSocketUrl } from "./ws";

function jsonResponse(body: unknown, init: ResponseInit = {}) {
  return new Response(JSON.stringify(body), {
    status: init.status ?? 200,
    headers: {
      "content-type": "application/json",
      ...(init.headers ?? {})
    }
  });
}

describe("device SDK client", () => {
  it("builds REST URLs from HTTPS origins and leaves absolute URLs intact", () => {
    expect(buildDeviceApiUrl("https://192.168.0.30/", "/api/system/info")).toBe(
      "https://192.168.0.30/api/system/info"
    );
    expect(buildDeviceApiUrl("https://matrixhub.local", "api/system/info")).toBe(
      "https://matrixhub.local/api/system/info"
    );
    expect(buildDeviceApiUrl("https://matrixhub.local", "https://other.local/api/status")).toBe(
      "https://other.local/api/status"
    );
  });

  it("maps device HTTPS origins to WSS endpoints", () => {
    expect(buildDeviceWebSocketUrl("https://192.168.0.30", "/ws/system")).toBe(
      "wss://192.168.0.30/ws/system"
    );
    expect(buildDeviceWebSocketUrl("matrixhub.local", "ws/system")).toBe(
      "wss://matrixhub.local/ws/system"
    );
    expect(buildDeviceWebSocketUrl("http://dev.local", "/ws/system")).toBe(
      "ws://dev.local/ws/system"
    );
    expect(buildCsiWebSocketUrl("https://192.168.0.30")).toBe("wss://192.168.0.30/ws/csi");
  });

  it("preserves backend error details and notifies on 401", async () => {
    const onUnauthorized = vi.fn();
    const fetchMock = vi.fn(async () =>
      jsonResponse(
        {
          error: "auth/session_expired",
          message: "Session expired"
        },
        { status: 401 }
      )
    );
    const client = createDeviceApiClient({
      baseUrl: "https://192.168.0.30",
      bearerToken: "token",
      fetch: fetchMock as unknown as typeof fetch,
      onUnauthorized
    });

    await expect(client.get("/api/system/info")).rejects.toMatchObject({
      status: 401,
      serverMessage: "Session expired",
      errorCode: "auth/session_expired"
    } satisfies Partial<ApiError>);
    expect(onUnauthorized).toHaveBeenCalledOnce();
  });

  it("classifies request failures for retry decisions", () => {
    expect(getRequestFailureKind(new DOMException("The operation timed out", "TimeoutError"))).toBe(
      "timeout"
    );
    expect(getRequestFailureKind(new DOMException("Aborted", "AbortError"))).toBe("abort");
    expect(getRequestFailureKind(new TypeError("Failed to fetch"))).toBe("network");
    expect(getRequestFailureKind(new Error("validation failed"))).toBeNull();
  });
});
