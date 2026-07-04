/**
 * @file LoggingConfig.cpp
 * @brief Logging configuration - now backed by RTC memory
 * 
 * On cold boot: loads from NVS Preferences → RTC
 * On warm boot: uses RTC directly (no NVS access)
 * On change: writes to RTC + NVS (backup)
 */

#include "App.h"
#include "../system/rtc/RtcConfig.h"
#include "core/config/ConfigManager.h"
#include <LittleFS.h>

namespace LoggingConfig {

LOG::Settings get() {
    LOG::Settings settings{};
    bool got = false;
    RTC::withConfig([&](const RTC::ConfigStore& store) {
        settings.level = store.logging.level;
        got = true;
    });
    if (!got) {
        settings.level = RTC::getConfig().logging.level;
    }
    return settings;
}

void begin() {
    // No specific initialization needed for RTC-backed config
}

bool setLevel(esp_log_level_t level) {
    const RTC::LoggingData previous = RTC::copyConfigSection(&RTC::ConfigStore::logging);

    if (!RTC::updateConfig([level](RTC::ConfigStore& store) {
        store.logging.level = level;
    })) {
        return false;
    }
    
    if (!CONFIG::save(LittleFS)) {
        RTC::updateConfig([previous](RTC::ConfigStore& store) {
            store.logging = previous;
        });
        return false;
    }

    return true;
}

}  // namespace LoggingConfig
