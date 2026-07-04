import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import { ApiError } from "./api/errors";
import type { LogListResponse, TailClearResponse, TailResponse } from "./types";

interface ErrorPayload {
  error?: string;
  message?: string;
}

async function toApiError(response: Response, fallbackMessage: string): Promise<ApiError> {
  let errorCode: string | undefined;
  let serverMessage: string | undefined;

  try {
    const contentType = response.headers.get("content-type");
    if (contentType?.includes("application/json")) {
      const body = (await response.json()) as ErrorPayload;
      if (typeof body.error === "string") {
        errorCode = body.error;
      }
      if (typeof body.message === "string") {
        serverMessage = body.message;
      }
    }
  } catch {
    // Fall back to the generic message when the response body is absent.
  }

  return new ApiError(response.status, fallbackMessage, serverMessage ?? errorCode, errorCode);
}

function buildLogFileQuery(path: string) {
  return `file=${encodeURIComponent(path)}`;
}

export class DeviceLogsApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getLogsList(): Promise<LogListResponse> {
    return this.client.get<LogListResponse>("/api/logs");
  }

  async downloadLog(path: string): Promise<Blob> {
    const url = `/api/logs/download?${buildLogFileQuery(path)}`;
    const response = await this.client.fetch(url);
    if (response.status === 404) {
      throw new ApiError(response.status, `GET ${url} failed`, "File not found");
    }
    if (!response.ok) {
      throw await toApiError(response, `GET ${url} failed`);
    }
    return response.blob();
  }

  async downloadLogBytes(path: string): Promise<ArrayBuffer> {
    const url = `/api/logs/download?${buildLogFileQuery(path)}`;
    const response = await this.client.fetch(url);
    if (response.status === 404) {
      throw new ApiError(response.status, `GET ${url} failed`, "File not found");
    }
    if (!response.ok) {
      throw await toApiError(response, `GET ${url} failed`);
    }
    return response.arrayBuffer();
  }

  async deleteLog(path: string): Promise<void> {
    await this.client.deleteVoid(`/api/logs/delete?${buildLogFileQuery(path)}`);
  }

  async getLogTail(options: { lines?: number; since?: number } = {}): Promise<TailResponse> {
    const params = new URLSearchParams();
    if (options.lines !== undefined) {
      params.set("lines", String(options.lines));
    }
    if (options.since !== undefined) {
      params.set("since", String(options.since));
    }
    const query = params.toString();
    const path = query ? `/rest/logs/tail?${query}` : "/rest/logs/tail";
    return this.client.get<TailResponse>(path);
  }

  async clearLogTail(): Promise<TailClearResponse> {
    const response = await this.client.fetch("/rest/logs/tail", { method: "DELETE" });
    if (!response.ok) {
      throw await toApiError(response, "DELETE /rest/logs/tail failed");
    }
    return response.json() as Promise<TailClearResponse>;
  }
}
