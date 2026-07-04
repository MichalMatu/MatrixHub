import { describe, expect, it } from "vitest";
import { ApiError } from "@matrixhub/device-sdk";
import { describeAppError, toAppError } from "./appError";

describe("app errors", () => {
  it("maps network-like failures to a friendly message", () => {
    expect(describeAppError(new TypeError("Failed to fetch"))).toContain(
      "Connection failed.",
    );
  });

  it("keeps auth/invalid_credentials API errors stable", () => {
    expect(
      toAppError(
        new ApiError(
          401,
          "Unauthorized",
          undefined,
          "auth/invalid_credentials",
        ),
      ).code,
    ).toBe("auth/invalid_credentials");
  });

  it("falls back to the original error message when it is meaningful", () => {
    expect(describeAppError(new Error("Custom failure"))).toBe(
      "Custom failure",
    );
  });
});
