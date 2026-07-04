export class ApiError extends Error {
  status: number;
  serverMessage?: string;
  errorCode?: string;

  constructor(status: number, message: string, serverMessage?: string, errorCode?: string) {
    super(message);
    this.name = "ApiError";
    this.status = status;
    this.serverMessage = serverMessage;
    this.errorCode = errorCode;
  }
}

export type RequestAbortKind = "abort" | "timeout";
export type RequestFailureKind = RequestAbortKind | "network";

function getErrorName(error: unknown): string | null {
  if (
    error &&
    typeof error === "object" &&
    "name" in error &&
    typeof (error as { name: unknown }).name === "string"
  ) {
    return (error as { name: string }).name;
  }
  return null;
}

function getErrorMessage(error: unknown): string {
  if (error instanceof Error) return error.message;
  if (typeof error === "string") return error;
  try {
    return JSON.stringify(error);
  } catch {
    return String(error);
  }
}

export function getRequestAbortKind(error: unknown): RequestAbortKind | null {
  const name = getErrorName(error);
  const message = getErrorMessage(error).toLowerCase();

  if (name === "TimeoutError") return "timeout";

  if (name === "AbortError") {
    if (message.includes("timeout") || message.includes("timed out")) return "timeout";
    return "abort";
  }

  if (message.includes("aborted")) return "abort";
  if (message.includes("timeout") || message.includes("timed out")) return "timeout";

  return null;
}

export function isNetworkLike(error: unknown): boolean {
  const name = getErrorName(error);
  const message = getErrorMessage(error).toLowerCase();
  if (name === "TypeError") {
    return (
      message.includes("failed to fetch") ||
      message.includes("networkerror") ||
      message.includes("load failed") ||
      message.includes("the network connection was lost")
    );
  }
  return (
    message.includes("failed to fetch") ||
    message.includes("networkerror") ||
    message.includes("load failed") ||
    message.includes("the network connection was lost")
  );
}

export function getRequestFailureKind(error: unknown): RequestFailureKind | null {
  const abortKind = getRequestAbortKind(error);
  if (abortKind) return abortKind;
  if (isNetworkLike(error)) return "network";
  return null;
}
