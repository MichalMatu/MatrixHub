import { ApiError } from "./errors";

export interface DeviceApiRequestInit<T> extends RequestInit {
  schema?: {
    parse: (value: unknown) => T;
  };
}

export interface DeviceApiClientOptions {
  baseUrl: string;
  bearerToken?: string;
  fetch?: typeof fetch;
  onUnauthorized?: () => void;
  timeoutMs?: number;
}

interface ParsedError {
  errorCode?: string;
  message?: string;
}

function normalizeBaseUrl(baseUrl: string) {
  return baseUrl.replace(/\/+$/, "");
}

export function buildDeviceApiUrl(baseUrl: string, path: string) {
  if (/^https?:\/\//i.test(path)) {
    return path;
  }

  if (!baseUrl) {
    return path;
  }

  const normalizedBase = normalizeBaseUrl(baseUrl);
  const normalizedPath = path.startsWith("/") ? path : `/${path}`;
  return `${normalizedBase}${normalizedPath}`;
}

async function parseErrorResponse(response: Response): Promise<ParsedError> {
  try {
    const contentType = response.headers.get("content-type");
    if (!contentType?.includes("application/json")) {
      return {};
    }

    const body = await response.json();
    if (!body || typeof body !== "object") {
      return {};
    }

    return {
      errorCode: typeof body.error === "string" ? body.error : undefined,
      message: typeof body.message === "string" ? body.message : undefined
    };
  } catch {
    return {};
  }
}

export function createDeviceApiClient(options: DeviceApiClientOptions) {
  const rawFetch = options.fetch ?? fetch;
  const defaultTimeoutMs = options.timeoutMs ?? 15000;

  const getHeaders = () => {
    const headers: Record<string, string> = {
      Accept: "application/json"
    };

    if (options.bearerToken) {
      headers.Authorization = `Bearer ${options.bearerToken}`;
    }

    return headers;
  };

  const fetchImpl = async (path: string, init?: RequestInit) => {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), defaultTimeoutMs);

    try {
      const response = await rawFetch(buildDeviceApiUrl(options.baseUrl, path), {
        ...init,
        signal: init?.signal ?? controller.signal
      });

      if (response.status === 401) {
        options.onUnauthorized?.();
      }

      return response;
    } finally {
      clearTimeout(timeoutId);
    }
  };

  const mergeHeaders = (init?: RequestInit): HeadersInit => {
    const defaults = getHeaders();
    const hasBody = init?.body !== undefined && init?.body !== null;
    if (hasBody && !(init?.body instanceof FormData)) {
      defaults["Content-Type"] = "application/json";
    }

    return {
      ...defaults,
      ...(init?.headers ?? {})
    };
  };

  return {
    async get<T>(path: string, init?: DeviceApiRequestInit<T>): Promise<T> {
      const response = await fetchImpl(path, {
        method: "GET",
        headers: mergeHeaders(init),
        ...init
      });
      if (!response.ok) {
        const { errorCode, message } = await parseErrorResponse(response);
        throw new ApiError(response.status, `GET ${path} failed`, message ?? errorCode, errorCode);
      }
      const data = await response.json();
      return init?.schema ? init.schema.parse(data) : (data as T);
    },

    async post<T>(path: string, body?: unknown, init?: DeviceApiRequestInit<T>): Promise<T> {
      const response = await fetchImpl(path, {
        method: "POST",
        headers: mergeHeaders(init),
        body:
          body === undefined || body instanceof FormData
            ? (body as BodyInit | undefined)
            : JSON.stringify(body),
        ...init
      });
      if (!response.ok) {
        const { errorCode, message } = await parseErrorResponse(response);
        throw new ApiError(response.status, `POST ${path} failed`, message ?? errorCode, errorCode);
      }
      const contentType = response.headers.get("content-type");
      if (contentType?.includes("application/json")) {
        const data = await response.json();
        return init?.schema ? init.schema.parse(data) : (data as T);
      }
      return undefined as T;
    },

    async postVoid(path: string, body?: unknown, init?: RequestInit): Promise<void> {
      const response = await fetchImpl(path, {
        method: "POST",
        headers: mergeHeaders(init),
        body:
          body === undefined || body instanceof FormData
            ? (body as BodyInit | undefined)
            : JSON.stringify(body),
        ...init
      });
      if (!response.ok) {
        const { errorCode, message } = await parseErrorResponse(response);
        throw new ApiError(response.status, `POST ${path} failed`, message ?? errorCode, errorCode);
      }
    },

    async deleteVoid(path: string, init?: RequestInit): Promise<void> {
      const response = await fetchImpl(path, {
        method: "DELETE",
        headers: mergeHeaders(init),
        ...init
      });
      if (!response.ok) {
        const { errorCode, message } = await parseErrorResponse(response);
        throw new ApiError(response.status, `DELETE ${path} failed`, message ?? errorCode, errorCode);
      }
    },

    async fetch(path: string, init?: RequestInit) {
      return fetchImpl(path, {
        ...init,
        headers: mergeHeaders(init)
      });
    }
  };
}
