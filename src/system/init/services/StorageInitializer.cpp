#include "StorageInitializer.h"

#include "core/config/ConfigManager.h"
#include "../../../config/System.h"
#include "../../../sensors/runtime/SensorState.h"
#include "../../../utils/hardware/I2cUtils.h"
#include "../../logging/Logging.h"
#include "../../rtc/RtcConfig.h"
#include "../../rtc/RtcConfigLoader.h"

#include <LittleFS.h>
#include <nvs_flash.h>
#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "StorageInit"

namespace SYSTEM {
namespace {

void initializeNvs() {
    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsErr = nvs_flash_init();
    }

    if (nvsErr != ESP_OK) {
        LOGE("[Phase1] NVS init failed: %s", esp_err_to_name(nvsErr));
        std::abort();
    }
}

void mountEarlyFilesystemIfNeeded() {
    if (RTC::isWarmBoot()) {
        LOGI("[Phase1] Warm boot fast path active - skipping early LittleFS mount");
        return;
    }

    const uint32_t fsStartMs = millis();
    bool fsMounted = LittleFS.begin(false);
    bool recoveredByFormat = false;

    if (!fsMounted && BOOT::FILESYSTEM::FORMAT_ON_FAIL) {
        LOGW("[Phase1] LittleFS mount failed on cold boot - formatting FS and restoring factory defaults");
        LittleFS.end();
        if (LittleFS.format()) {
            fsMounted = LittleFS.begin(false);
            recoveredByFormat = fsMounted;
        } else {
            LOGE("[Phase1] LittleFS format-on-fail recovery failed");
        }
    }

    LOGI("[Phase1] LittleFS mount: %s (%lu ms)", fsMounted ? "OK" : "FAILED", millis() - fsStartMs);
    if (!fsMounted) {
        LOGE("[Phase1] LittleFS mount failed on cold boot; cannot recover config");
        std::abort();
    }
    if (recoveredByFormat) {
        LOGW("[Phase1] LittleFS reformatted after mount failure; continuing with factory defaults");
    }
}

void initializeRtcConfig() {
    const uint32_t rtcStartMs = millis();
    const bool rtcReady = RTC::initConfig(LittleFS);
    LOGI("[Phase1] RTC settings init: %s (%lu ms)", rtcReady ? "OK" : "FAILED", millis() - rtcStartMs);
    if (!rtcReady) {
        LOGE("[Phase1] RTC configuration init failed");
        std::abort();
    }

    LOGI("[Phase1] Boot path latched: %s", RTC::wasWarmBootPathUsed() ? "warm" : "cold");
}

void hydratePsramOnlyConfigIfNeeded() {
    if (!RTC::wasWarmBootPathUsed()) {
        return;
    }

    const uint32_t fsStartMs = millis();
    const bool fsMounted = LittleFS.begin(false);
    LOGI("[Phase1] Warm boot LittleFS mount for PSRAM config: %s (%lu ms)",
         fsMounted ? "OK" : "FAILED",
         millis() - fsStartMs);
    if (!fsMounted) {
        // Warm boot skips the early mount path, so failing here would leave later
        // services running with a "successful" warm boot but no filesystem behind it.
        LOGE("[Phase1] Warm boot cannot continue without LittleFS mount");
        std::abort();
    }

    const uint32_t loadStartMs = millis();
    const bool hydrated = CONFIG::loadPsramOnly(LittleFS);
    LOGI("[Phase1] Warm boot PSRAM-only hydration: %s (%lu ms)",
         hydrated ? "OK" : "SKIPPED",
         millis() - loadStartMs);
    if (!hydrated) {
        // PSRAM-only hydration is just the fast path. Fall back to a full FS reload
        // instead of continuing with a partially restored warm-boot configuration.
        LOGW("[Phase1] Warm boot hydration incomplete - falling back to full FS reload");
        if (!RTC::reloadAllFromFS(LittleFS)) {
            LOGE("[Phase1] Warm boot full FS reload fallback failed");
            std::abort();
        }
    }
}

void createCoreLocks() {
    if (!g_fsMutex) {
        g_fsMutex = xSemaphoreCreateMutex();
        if (!g_fsMutex) {
            LOGE("[Phase1] Failed to create global FS mutex");
            std::abort();
        }
    }

    SENSORS::SensorState::createInitMutex();
    UTILS::HARDWARE::I2cUtils::createBusLock();
}

}  // namespace

void StorageInitializer::initialize() {
    initializeNvs();
    mountEarlyFilesystemIfNeeded();
    RTC::createLock();
    initializeRtcConfig();
    hydratePsramOnlyConfigIfNeeded();
    createCoreLocks();

    LOGI("[Phase1] Storage & Mutexes initialized (NVS + RTC fast/cold path)");
}

}  // namespace SYSTEM
