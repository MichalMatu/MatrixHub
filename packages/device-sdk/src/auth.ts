import { createDeviceApiClient } from "./api/client";
import { ApiError, getRequestFailureKind } from "./api/errors";
import type { AccessTokenPayload, SignInRequest, SignInResponse } from "./types";

function decodeBase64Url(input: string) {
  const normalized = input.replace(/-/g, "+").replace(/_/g, "/");
  const padded = normalized.padEnd(Math.ceil(normalized.length / 4) * 4, "=");

  if (typeof atob === "function") {
    return atob(padded);
  }

  const globalBuffer = (globalThis as { Buffer?: { from: (value: string, encoding: string) => { toString: (encoding: string) => string } } }).Buffer;
  if (globalBuffer) {
    return globalBuffer.from(padded, "base64").toString("utf-8");
  }

  throw new Error("Base64 decoding is not available in this environment.");
}

export function decodeAccessTokenPayload(token: string): AccessTokenPayload | null {
  if (!token) return null;

  try {
    const parts = token.split(".");
    if (parts.length < 2) return null;
    const payload = decodeBase64Url(parts[1]);
    const parsed = JSON.parse(payload);
    if (!parsed || typeof parsed !== "object") {
      return null;
    }

    return {
      username: typeof parsed.username === "string" ? parsed.username : undefined,
      admin: typeof parsed.admin === "boolean" ? parsed.admin : undefined,
      iat: typeof parsed.iat === "number" ? parsed.iat : undefined
    };
  } catch {
    return null;
  }
}

export async function signInToDevice(options: {
  baseUrl: string;
  credentials: SignInRequest;
  fetch?: typeof fetch;
}): Promise<SignInResponse | null> {
  const client = createDeviceApiClient({
    baseUrl: options.baseUrl,
    bearerToken: "",
    fetch: options.fetch
  });

  const SIGN_IN_RETRY_DELAY_MS = 300;
  const MAX_RETRY_ATTEMPTS = 1;

  for (let attempt = 0; attempt <= MAX_RETRY_ATTEMPTS; attempt += 1) {
    try {
      return await client.post<SignInResponse>("/rest/signIn", options.credentials);
    } catch (error) {
      if (
        error instanceof ApiError &&
        error.status === 401 &&
        (!error.errorCode || error.errorCode === "auth/invalid_credentials")
      ) {
        return null;
      }

      const failureKind = getRequestFailureKind(error);
      const shouldRetry =
        attempt < MAX_RETRY_ATTEMPTS && (failureKind === "timeout" || failureKind === "network");
      if (shouldRetry) {
        await new Promise((resolve) => setTimeout(resolve, SIGN_IN_RETRY_DELAY_MS));
        continue;
      }

      throw error;
    }
  }

  throw new Error("auth/sign_in_unreachable");
}
