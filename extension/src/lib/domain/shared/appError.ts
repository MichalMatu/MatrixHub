import { ApiError, getRequestFailureKind } from "@matrixhub/device-sdk";
import type { TranslationKey } from "$lib/i18n/catalog";
import { t } from "$lib/i18n/runtime";

export type AppErrorCode =
  | "permissions/host_denied"
  | "validation/device_address_required"
  | "validation/device_address_invalid"
  | "validation/username_required"
  | "validation/password_required"
  | "validation/hostname_required"
  | "validation/hostname_too_short"
  | "validation/hostname_too_long"
  | "validation/hostname_invalid"
  | "validation/connection_mode_invalid"
  | "auth/invalid_credentials"
  | "request/network"
  | "request/timeout"
  | "request/failed";

function defaultMessage(code: AppErrorCode) {
  return t(`errors.${code}` as TranslationKey);
}

function getRawMessage(error: unknown) {
  if (error instanceof Error) return error.message;
  if (typeof error === "string") return error;

  try {
    return JSON.stringify(error);
  } catch {
    return String(error);
  }
}

export class AppError extends Error {
  readonly code: AppErrorCode;
  readonly cause: unknown;

  constructor(
    code: AppErrorCode,
    message = defaultMessage(code),
    cause?: unknown,
  ) {
    super(message);
    this.name = "AppError";
    this.code = code;
    this.cause = cause;
  }
}

export function createAppError(
  code: AppErrorCode,
  message?: string,
  cause?: unknown,
) {
  return new AppError(code, message ?? defaultMessage(code), cause);
}

export function toAppError(
  error: unknown,
  fallbackCode: AppErrorCode = "request/failed",
): AppError {
  if (error instanceof AppError) {
    return error;
  }

  const failureKind = getRequestFailureKind(error);
  if (failureKind === "network") {
    return createAppError("request/network", undefined, error);
  }
  if (failureKind === "timeout") {
    return createAppError("request/timeout", undefined, error);
  }

  if (
    error instanceof ApiError &&
    error.errorCode === "auth/invalid_credentials"
  ) {
    return createAppError("auth/invalid_credentials", undefined, error);
  }

  const rawMessage = getRawMessage(error);
  return createAppError(
    fallbackCode,
    rawMessage && rawMessage !== "[object Object]" ? rawMessage : undefined,
    error,
  );
}

export function describeAppError(
  error: unknown,
  fallbackCode: AppErrorCode = "request/failed",
) {
  return toAppError(error, fallbackCode).message;
}
