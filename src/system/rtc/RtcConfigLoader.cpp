/**
 * @file RtcConfigLoader.cpp
 * @brief Boot-path selector for retained RTC restore vs LittleFS reload
 */

#include "RtcConfigLoader.h"
#include "RtcConfig.h"
#include "../../config/App.h"
#include "../../config/Hardware.h"
#include "core/config/ConfigManager.h"
#include "../logging/Logging.h"
#include <esp_system.h>

#undef LOG_TAG
#define LOG_TAG "RtcLoader"

namespace RTC {

namespace {
bool s_warmBootPathUsed = false;

void applyLoadedLoggingLevel() {
    LOG::Logging::setLevel(getConfig().logging.level);
}
}  // namespace

bool isWarmBoot() {
    return (esp_reset_reason() == ESP_RST_DEEPSLEEP) && isValid();
}

bool wasWarmBootPathUsed() {
    return s_warmBootPathUsed;
}

bool initConfig(FS& fs) {
    esp_reset_reason_t reason = esp_reset_reason();
    const bool deepSleepReset = (reason == ESP_RST_DEEPSLEEP);
    const bool rtcValidAtBoot = isValid();
    
    if (deepSleepReset && rtcValidAtBoot) {
        s_warmBootPathUsed = true;
        applyLoadedLoggingLevel();

        // Warm boot fast path: the retained snapshot already restored the
        // PSRAM working copy, so we only validate runtime-only RTC structures
        // and skip the early LittleFS config reload.
        LOGI("Warm boot detected - using RTC config (skipping FS)");
        
        // Validate runtime stats/sensors as well
        validateRuntimeData();
        consumeMaintenanceWakeFlag();
        
        logStatus();
        return true;
    }
    
    s_warmBootPathUsed = false;

    if (deepSleepReset && !rtcValidAtBoot) {
        LOGW("Deep-sleep reset detected, but RTC config is invalid - falling back to FS reload");
    }

    // Cold boot, or deep-sleep wake with an invalid retained snapshot:
    // rebuild the PSRAM working copy from defaults and then load LittleFS.
    LOGI("Cold boot (reason=%d) - loading config from FS to RTC", reason);
    return reloadAllFromFS(fs);
}

bool reloadAllFromFS(FS& fs) {
    s_warmBootPathUsed = false;

    // Start from compile-time defaults so a genuinely missing config file has
    // an intentional factory path. A present malformed/critical document is
    // rejected below instead of booting with those empty alarm defaults.
    initDefaults();
    
    LOGI("Forcing full config reload from FS...");
    uint32_t totalStart = millis();
    
    uint32_t start = millis();
    CONFIG::LoadFailure loadFailure = CONFIG::LoadFailure::None;
    const bool loadedFromFs = CONFIG::load(fs, &loadFailure);
    LOGI("  Settings JSON load: %s (%lu ms)", loadedFromFs ? "OK" : "FAILED", millis() - start);
    if (loadFailure == CONFIG::LoadFailure::CriticalSection ||
        loadFailure == CONFIG::LoadFailure::InvalidDocument) {
        // Only a genuinely absent file means an intentional factory-default
        // boot. A present but malformed document (or a missing/invalid alarm
        // section inside it) must not silently become an empty alarm store.
        // Durable backup recovery is a later hardening layer; until then the
        // safe behavior is to refuse boot and preserve the damaged evidence.
        LOGE("  Existing configuration rejected; refusing to complete boot config");
        return false;
    }
    if (!loadedFromFs) {
        LOGW("  Using factory defaults in RTC (config file missing)");
    }
    
    // Mark the freshly built working copy as valid for later warm-boot use.
    markValid();
    applyLoadedLoggingLevel();
    
    LOGI("RTC config ready (%u bytes used)", sizeof(ConfigStore));
    logStatus();
    LOGI("Full FS->RTC sync complete in %lu ms", millis() - totalStart);
    return true;
}

}  // namespace RTC
