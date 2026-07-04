import { describe, expect, it, vi } from "vitest";

import { decodeAccessTokenPayload, signInToDevice } from "./auth";

function jsonResponse(body: unknown, init: ResponseInit = {}) {
  return new Response(JSON.stringify(body), {
    status: init.status ?? 200,
    headers: {
      "content-type": "application/json",
      ...(init.headers ?? {})
    }
  });
}

function createJwtPayload(payload: Record<string, unknown>) {
  const encoded = btoa(JSON.stringify(payload))
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=+$/g, "");
  return `header.${encoded}.signature`;
}

describe("device SDK auth", () => {
  it("decodes access token payloads without validating the signature", () => {
    expect(
      decodeAccessTokenPayload(
        createJwtPayload({
          username: "admin",
          admin: true,
          iat: 123
        })
      )
    ).toEqual({
      username: "admin",
      admin: true,
      iat: 123
    });

    expect(decodeAccessTokenPayload("not-a-jwt")).toBeNull();
  });

  it("returns null for invalid credentials", async () => {
    const fetchMock = vi.fn(async () =>
      jsonResponse(
        {
          error: "auth/invalid_credentials",
          message: "Invalid credentials"
        },
        { status: 401 }
      )
    );

    await expect(
      signInToDevice({
        baseUrl: "https://192.168.0.30",
        credentials: { username: "admin", password: "wrong" },
        fetch: fetchMock as unknown as typeof fetch
      })
    ).resolves.toBeNull();
  });

  it("retries sign-in once after a network-like failure", async () => {
    const fetchMock = vi
      .fn()
      .mockRejectedValueOnce(new TypeError("Failed to fetch"))
      .mockResolvedValueOnce(jsonResponse({ access_token: "token" }));

    await expect(
      signInToDevice({
        baseUrl: "https://192.168.0.30",
        credentials: { username: "admin", password: "admin" },
        fetch: fetchMock as unknown as typeof fetch
      })
    ).resolves.toEqual({ access_token: "token" });

    expect(fetchMock).toHaveBeenCalledTimes(2);
  });
});
