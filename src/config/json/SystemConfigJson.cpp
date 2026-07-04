#include "SystemConfigJson.h"
#include "../App.h"
#include "../../system/health/heartbeat/HeartbeatConfigSanitizer.h"
#include "../../system/health/heartbeat/HeartbeatConfigStore.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/logging/Logging.h"
#include "../System.h"
#include <algorithm>

namespace CONFIG {
namespace JSON {





// ---------------------------------------------------------------------------
// deserializeHeartbeat — shared by loadHeartbeat (file) AND API endpoint.
// Overwrites entire struct (full-replace, not partial-update).
// ---------------------------------------------------------------------------
void deserializeHeartbeat(JsonObject& obj, RTC::HeartbeatData& h) {
    if (obj[Keys::kIntervalMs].is<uint32_t>()) {
        uint32_t interval = obj[Keys::kIntervalMs];
        h.intervalMs = std::max(LIMITS::HEARTBEAT::MIN_INTERVAL_MS, 
                              std::min(interval, LIMITS::HEARTBEAT::MAX_INTERVAL_MS));
    }
    
    if (obj[Keys::kSlots].is<JsonArray>()) {
        // Reset slots if explicitly provided in array
        for (uint8_t i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
            memset(&h.slots[i], 0, sizeof(RTC::HeartbeatSlot));
        }

        uint8_t idx = 0;
        for (JsonObject slot : obj[Keys::kSlots].as<JsonArray>()) {
            if (idx >= RTC::kMaxHeartbeatSlots) break;
            
            auto& s = h.slots[idx];
            if (slot[Keys::kEnabled].is<bool>()) {
                bool v = slot[Keys::kEnabled].as<bool>();
                s.enabled = v;
            }
            
            if (const char* name = slot[Keys::kName] | (const char*)nullptr) {
                strlcpy(s.name, name, sizeof(s.name));
            }
            
            if (const char* url = slot[Keys::kUrl] | (const char*)nullptr) {
                strlcpy(s.url, url, sizeof(s.url));
            }

            if (slot[Keys::kAllowInsecure].is<bool>()) {
                s.allowInsecure = slot[Keys::kAllowInsecure].as<bool>();
            }

            idx++;
        }
    }

    SYSTEM::HEARTBEAT_DETAIL::sanitizeConfig(h);
}

void loadHeartbeat(JsonObject& obj) {
    SYSTEM::HEARTBEAT_CONFIG::update([&](RTC::HeartbeatData& heartbeat) {
        deserializeHeartbeat(obj, heartbeat);
    });
}

void saveHeartbeat(JsonObject& obj) {
    RTC::HeartbeatData h = SYSTEM::HEARTBEAT_CONFIG::copy();
    
    obj[Keys::kIntervalMs] = h.intervalMs;
    
    JsonArray slots = obj[Keys::kSlots].to<JsonArray>();
    for (uint8_t i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
        const auto& s = h.slots[i];
        JsonObject slot = slots.add<JsonObject>();
        slot[Keys::kEnabled] = s.enabled;

        char safeName[sizeof(s.name)];
        strlcpy(safeName, s.name, sizeof(safeName));
        if (!SYSTEM::HEARTBEAT_DETAIL::isSafeHeartbeatLabel(safeName, sizeof(safeName), nullptr)) {
            safeName[0] = '\0';
        }

        char safeUrl[sizeof(s.url)];
        strlcpy(safeUrl, s.url, sizeof(safeUrl));
        size_t dummyLen = 0;
        if (!SYSTEM::HEARTBEAT_DETAIL::isSafeHeartbeatUrl(safeUrl, sizeof(safeUrl), &dummyLen)) {
            safeUrl[0] = '\0';
        }

        // Force ArduinoJson to duplicate strings (avoid dangling stack pointers on v7)
        slot[Keys::kName].set(String(safeName));
        slot[Keys::kUrl].set(String(safeUrl));
        slot[Keys::kAllowInsecure] = s.allowInsecure;
    }
}

// ---------------------------------------------------------------------------
// deserializeUdpPusher — shared by loadUdpPusher (file) AND API endpoint.
// Overwrites entire struct (full-replace, not partial-update).
// ---------------------------------------------------------------------------
void deserializeUdpPusher(JsonObject& obj, RTC::UdpPusherData& u) {
    if (obj[Keys::kEnabled].is<bool>()) {
        bool v = obj[Keys::kEnabled].as<bool>();
        u.enabled = v;
    }
    if (obj[Keys::kPort].is<uint16_t>()) {
        uint16_t v = obj[Keys::kPort].as<uint16_t>();
        u.port = std::clamp<uint16_t>(v, LIMITS::UDP_PUSHER::MIN_PORT, LIMITS::UDP_PUSHER::MAX_PORT);
    }
    
    if (obj[Keys::kIntervalMs].is<uint32_t>()) {
        uint32_t interval = obj[Keys::kIntervalMs];
        u.intervalMs = std::max(LIMITS::UDP_PUSHER::MIN_INTERVAL_MS, 
                              std::min(interval, LIMITS::UDP_PUSHER::MAX_INTERVAL_MS));
    }
    
    // Parse format string -> enum
    if (const char* fmtStr = obj[Keys::kFormat] | (const char*)nullptr) {
        String fmt(fmtStr);
        if (fmt == "json") u.format = RTC::UdpFormat::Json;
        else if (fmt == "csv") u.format = RTC::UdpFormat::Csv;
        else if (fmt == "line") u.format = RTC::UdpFormat::LineProtocol;
    }
    
    if (const char* host = obj[Keys::kHost] | (const char*)nullptr) {
                strlcpy(u.host, host, sizeof(u.host));
    }
}

void loadUdpPusher(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::udpPusher, [&](RTC::UdpPusherData& udpPusher) {
        deserializeUdpPusher(obj, udpPusher);
    });
}

void saveUdpPusher(JsonObject& obj) {
    RTC::UdpPusherData u{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        u = cfg.udpPusher;
    });
    
    obj[Keys::kEnabled] = u.enabled;
    obj[Keys::kHost].set(String(u.host)); // duplicate into JsonDocument
    obj[Keys::kPort] = u.port;
    obj[Keys::kIntervalMs] = u.intervalMs;
    
    // Format enum -> string
    switch (u.format) {
        case RTC::UdpFormat::Json: obj[Keys::kFormat] = "json"; break;
        case RTC::UdpFormat::Csv: obj[Keys::kFormat] = "csv"; break;
        default: obj[Keys::kFormat] = "line"; break;
    }
}

// ---------------------------------------------------------------------------
// deserializeLogging — shared by loadLogging (file) AND API endpoint.
// Reads level as string (e.g. "debug", "info"). Partial-update safe.
// ---------------------------------------------------------------------------
void deserializeLogging(JsonObject& obj, RTC::LoggingData& data) {
    // Use | nullptr instead of is<const char*>(): in ArduinoJson v7, owned strings
    // (parsed from a body copy) pass is<JsonString>() but NOT is<const char*>(),
    // so the old check silently skipped the level field on every API call.
    const char* lvl = obj[Keys::kLevel] | (const char*)nullptr;
    if (lvl && lvl[0] != '\0') {
        // String format (canonical): "verbose", "debug", "info", "warn", "error", "none"
        data.level = LOG::Logging::stringToLevel(lvl, data.level);
    } else if (obj[Keys::kLevel].is<int>()) {
        // Backward compatibility: old config.json may have int (0-5)
        int v = obj[Keys::kLevel] | static_cast<int>(RTC::Defaults::Logging::Level);
        if (v >= ESP_LOG_NONE && v <= ESP_LOG_VERBOSE) {
            data.level = static_cast<esp_log_level_t>(v);
        }
    }
}

void loadLogging(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::logging, [&](RTC::LoggingData& logging) {
        deserializeLogging(obj, logging);
    });
}

void saveLogging(JsonObject& obj) {
    RTC::LoggingData l{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        l = cfg.logging;
    });
    // Always save as string for consistency with API and frontend
    obj[Keys::kLevel].set(String(LOG::Logging::levelToString(l.level)));
}





} // namespace JSON
} // namespace CONFIG
