import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import { ApiError } from "./api/errors";
import type { MacroActionResponse, MacroSettings, ScriptFile, ScriptStatus } from "./types";

async function toApiError(response: Response, fallbackMessage: string): Promise<ApiError> {
  try {
    const contentType = response.headers.get("content-type");
    if (contentType?.includes("application/json")) {
      const body = (await response.json()) as { error?: string; message?: string };
      const errorCode = typeof body.error === "string" ? body.error : undefined;
      const message = typeof body.message === "string" ? body.message : undefined;
      return new ApiError(response.status, fallbackMessage, message ?? errorCode, errorCode);
    }
  } catch {
    // Fall through to the generic error.
  }

  return new ApiError(response.status, fallbackMessage);
}

export class DeviceMacrosApi {
  private readonly basePath = "/api/macros";
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async listScripts(signal?: AbortSignal): Promise<ScriptFile[]> {
    return this.client.get<ScriptFile[]>(this.basePath, { signal });
  }

  async uploadScript(filename: string, content: string): Promise<MacroActionResponse> {
    return this.client.post<MacroActionResponse>(this.basePath, { filename, content });
  }

  async deleteScript(filename: string): Promise<MacroActionResponse> {
    return this.client.post<MacroActionResponse>(`${this.basePath}/delete`, { name: filename });
  }

  async runScript(filename: string): Promise<MacroActionResponse> {
    return this.client.post<MacroActionResponse>(`${this.basePath}/run`, { name: filename });
  }

  async stopScript(): Promise<MacroActionResponse> {
    return this.client.post<MacroActionResponse>(`${this.basePath}/stop`);
  }

  async getStatus(signal?: AbortSignal): Promise<ScriptStatus> {
    return this.client.get<ScriptStatus>(`${this.basePath}/status`, { signal });
  }

  async getScriptContent(filename: string, signal?: AbortSignal): Promise<string> {
    const path = `${this.basePath}/content?name=${encodeURIComponent(filename)}`;
    const response = await this.client.fetch(path, { signal });
    if (!response.ok) {
      throw await toApiError(response, `GET ${path} failed`);
    }
    return response.text();
  }

  async getSettings(signal?: AbortSignal): Promise<MacroSettings> {
    return this.client.get<MacroSettings>(`${this.basePath}/settings`, { signal });
  }

  async saveSettings(settings: MacroSettings): Promise<MacroSettings> {
    return this.client.post<MacroSettings>(`${this.basePath}/settings`, settings);
  }
}
