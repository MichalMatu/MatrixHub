import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type {
  DiagnosticsEndpointsResponse,
  DiagnosticsFeaturesResponse,
  DiagnosticsHeapResponse,
  DiagnosticsMutexesResponse,
  DiagnosticsSummaryResponse,
  TasksResponse
} from "./types";

export class DeviceDiagnosticsApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSummary(): Promise<DiagnosticsSummaryResponse> {
    return this.client.get<DiagnosticsSummaryResponse>("/api/diagnostics/summary");
  }

  async getHeap(): Promise<DiagnosticsHeapResponse> {
    return this.client.get<DiagnosticsHeapResponse>("/api/diagnostics/heap");
  }

  async getTasks(options: { details?: boolean } = {}): Promise<TasksResponse> {
    const path = options.details ? "/api/diagnostics/tasks?details=1" : "/api/diagnostics/tasks";
    return this.client.get<TasksResponse>(path);
  }

  async getMutexes(): Promise<DiagnosticsMutexesResponse> {
    return this.client.get<DiagnosticsMutexesResponse>("/api/diagnostics/mutexes");
  }

  async getEndpoints(): Promise<DiagnosticsEndpointsResponse> {
    return this.client.get<DiagnosticsEndpointsResponse>("/api/diagnostics/endpoints");
  }

  async getFeatures(): Promise<DiagnosticsFeaturesResponse> {
    return this.client.get<DiagnosticsFeaturesResponse>("/api/diagnostics/features");
  }
}
