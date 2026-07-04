import {
  createDeviceApiClient,
  type DeviceApiClientOptions,
} from "./api/client";
import { ApiError } from "./api/errors";
import type {
  ApSettings,
  ApStatus,
  NetworkListResponse,
  NtpSettings,
  NtpStatus,
  WifiSettings,
  WifiStatus,
} from "./types";

function stripUndefined<T extends Record<string, unknown>>(value: T) {
  const result: Record<string, unknown> = {};
  Object.entries(value).forEach(([key, entryValue]) => {
    if (entryValue !== undefined) {
      result[key] = entryValue;
    }
  });
  return result;
}

interface ErrorPayload {
  error?: string;
  message?: string;
}

async function toApiError(
  response: Response,
  fallbackMessage: string,
): Promise<ApiError> {
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
    // Keep the fallback message when the backend does not return JSON.
  }

  return new ApiError(
    response.status,
    fallbackMessage,
    serverMessage ?? errorCode,
    errorCode,
  );
}

export class DeviceNetworkApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getWifiStatus(): Promise<WifiStatus> {
    return this.client.get<WifiStatus>("/rest/wifiStatus");
  }

  async getWifiSettings(): Promise<WifiSettings> {
    return this.client.get<WifiSettings>("/rest/wifiSettings");
  }

  async saveWifiSettings(settings: WifiSettings): Promise<WifiSettings> {
    const payload = {
      ...settings,
      hostname: settings.hostname || "matrixhub",
      wifi_networks: settings.wifi_networks.map((network) =>
        stripUndefined(network as unknown as Record<string, unknown>),
      ),
    };

    return this.client.post<WifiSettings>("/rest/wifiSettings", payload);
  }

  async scanNetworks(): Promise<void> {
    const response = await this.client.fetch("/rest/scanNetworks", {
      method: "GET",
    });
    if (!response.ok) {
      throw await toApiError(response, "GET /rest/scanNetworks failed");
    }
  }

  async listNetworks(): Promise<NetworkListResponse> {
    return this.client.get<NetworkListResponse>("/rest/listNetworks");
  }

  async getApStatus(): Promise<ApStatus> {
    return this.client.get<ApStatus>("/rest/apStatus");
  }

  async getApSettings(): Promise<ApSettings> {
    return this.client.get<ApSettings>("/rest/apSettings");
  }

  async getNtpStatus(): Promise<NtpStatus> {
    return this.client.get<NtpStatus>("/rest/ntpStatus");
  }

  async getNtpSettings(): Promise<NtpSettings> {
    return this.client.get<NtpSettings>("/rest/ntpSettings");
  }

  async saveNtpSettings(settings: NtpSettings): Promise<NtpSettings> {
    return this.client.post<NtpSettings>("/rest/ntpSettings", settings);
  }

  async setTime(localTime: string): Promise<void> {
    await this.client.postVoid("/rest/time", { local_time: localTime });
  }
}
