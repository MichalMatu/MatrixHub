import { createDeviceApiClient, type DeviceApiClientOptions } from "./api/client";
import type { NotificationSettings, NotificationTestResult } from "./types";

export class DeviceNotificationsApi {
  private client;

  constructor(options: DeviceApiClientOptions) {
    this.client = createDeviceApiClient(options);
  }

  async getSettings(): Promise<NotificationSettings> {
    return this.client.get<NotificationSettings>("/api/notifications/settings");
  }

  async updateSettings(settings: Partial<NotificationSettings>): Promise<NotificationSettings> {
    return this.client.post<NotificationSettings>("/api/notifications/settings", settings);
  }

  async testTelegram(message: string, signal?: AbortSignal): Promise<NotificationTestResult> {
    return this.client.post<NotificationTestResult>(
      "/api/notifications/telegram/test",
      { text: message },
      { signal }
    );
  }

  async testWebhook(content: string, signal?: AbortSignal): Promise<NotificationTestResult> {
    return this.client.post<NotificationTestResult>(
      "/api/notifications/webhook/test",
      { content },
      { signal }
    );
  }

  async testPushover(message: string, signal?: AbortSignal): Promise<NotificationTestResult> {
    return this.client.post<NotificationTestResult>(
      "/api/notifications/pushover/test",
      { message },
      { signal }
    );
  }
}
