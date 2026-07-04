/**
 * @file NotificationSettingsAdapter.h
 * @brief JSON adapter for NotificationData (extracted from NotificationSettingsService)
 *
 * Maps RTC::NotificationData schema to/from JSON for API endpoints.
 */

#pragma once

#include "../../system/rtc/RtcConfig.h"
#include <core/StatefulService.h>
#include <ArduinoJson.h>
#include <cstring>
#include <string_view>

/**
 * @brief JSON adapter for unified notification settings.
 *
 * Handles API field mapping for Telegram, Webhook, and Pushover settings.
 */
struct NotificationSettingsAdapter {
    /**
     * @brief Convert notification settings to JSON object.
     */
    static void read(RTC::NotificationData& settings, JsonObject& root);

    /**
     * @brief Update notification settings from JSON object.
     * @return CHANGED if any field was modified, UNCHANGED otherwise.
     */
    static StateUpdateResult update(JsonObject& root, RTC::NotificationData& settings, std::string_view originId);

private:
    /**
     * @brief Helper to update boolean fields if present.
     * @return true if changed
     */
    static bool updateBoolField(JsonObject& root, const char* key, bool& dest);

    /**
     * @brief Helper to update string fields if changed.
     * @return true if changed
     */
    static bool updateString(char* dest, const char* src, size_t maxLen);

    /**
     * @brief Helper to update string fields from JSON if present.
     * @return true if changed
     */
    static bool updateStringField(JsonObject& root, const char* key, char* dest, size_t maxLen);
};
