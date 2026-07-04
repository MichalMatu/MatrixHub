#pragma once

#include <Arduino.h>

namespace RTC {

/**
 * Unified Notification Settings.
 *
 * The full configuration now lives in a PSRAM-backed store and is serialized
 * to LittleFS. Keep the type in RTC namespace because the rest of the codebase
 * already uses it as the canonical schema.
 */
struct __attribute__((packed)) NotificationData {
    bool telegramEnabled = false;
    bool webhookEnabled = false;

    // Telegram Configuration
    char botToken[64] = {0};
    char chatId[24] = {0};  // Sized for group IDs (-100xxxx) - matches MessageQueue
    bool commandsEnabled = false; // Poll for commands if Telegram is active
    // Poll interval is hardcoded to 10s to simplify config
    
    // Webhook Configuration
    char webhookUrl[256] = {0};
    
    // Pushover Configuration
    bool pushoverEnabled = false;
    char pushoverUserKey[32] = {0}; // 30 chars + null
    char pushoverApiToken[32] = {0}; // 30 chars + null

    bool isTelegramReady() const {
        return telegramEnabled && botToken[0] != '\0';
    }

    bool isWebhookReady() const {
        return webhookEnabled && webhookUrl[0] != '\0';
    }
    
    bool isPushoverReady() const {
        return pushoverEnabled && pushoverUserKey[0] != '\0' && pushoverApiToken[0] != '\0';
    }

    bool isConfigured() const {
        return isTelegramReady() || isWebhookReady() || isPushoverReady();
    }
    
    /** Check if chatId is set (for outbound notifications) */
    bool hasChatId() const {
        return chatId[0] != '\0';
    }
};

/**
 * Minimal retained notification summary kept in RTC.
 *
 * This is enough for boot diagnostics and coarse retained state checks without
 * pinning large secrets/URLs in RTC slow memory.
 */
struct __attribute__((packed)) NotificationSummaryData {
    bool telegramEnabled = false;
    bool webhookEnabled = false;
    bool pushoverEnabled = false;
    bool commandsEnabled = false;
    bool hasBotToken = false;
    bool hasChatId = false;
    bool hasWebhookUrl = false;
    bool hasPushoverCreds = false;

    bool isTelegramReady() const {
        return telegramEnabled && hasBotToken;
    }

    bool isWebhookReady() const {
        return webhookEnabled && hasWebhookUrl;
    }

    bool isPushoverReady() const {
        return pushoverEnabled && hasPushoverCreds;
    }

    bool isConfigured() const {
        return isTelegramReady() || isWebhookReady() || isPushoverReady();
    }
};

} // namespace RTC
