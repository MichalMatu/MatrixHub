#include "core/config/persistence/ConfigPersistence.h"

#include "config/Network.h"
#include "core/config/ConfigCommon.h"
#include "system/logging/Logging.h"
#include "system/memory/PsramAllocator.h"
#include "system/utils/ScopeLock.h"
#include <ArduinoJson.h>

#undef LOG_TAG
#define LOG_TAG "ConfigPers"

namespace {

bool validateConfigFileSize(size_t fileSize, const char* phase) {
    if (fileSize == 0) {
        LOGW("%s config file is empty", phase);
        return false;
    }
    if (fileSize > CONFIG::kConfigFileMaxBytes) {
        LOGE("%s config file too large: %u bytes (max %u)",
             phase,
             static_cast<unsigned>(fileSize),
             static_cast<unsigned>(CONFIG::kConfigFileMaxBytes));
        return false;
    }
    return true;
}

}  // namespace

namespace CONFIG::Persistence {

bool readConfigDocument(FS& fs, SYSTEM::SpiRamJsonDocument& doc, const char* phase) {
    File file = fs.open(kConfigFile, "r");
    if (!file) {
        LOGW("Failed to open config file");
        return false;
    }

    const size_t fileSize = file.size();
    if (!validateConfigFileSize(fileSize, phase)) {
        file.close();
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOGW("%s JSON parse error: %s", phase, error.c_str());
        return false;
    }
    if (doc.overflowed()) {
        LOGE("Config JSON document overflowed during %s; data may be truncated", phase);
        return false;
    }

    LOGD("Config JSON size (%s): %u bytes (buffer %u bytes)",
         phase,
         static_cast<unsigned>(fileSize),
         static_cast<unsigned>(kConfigDocSize));
    return true;
}

bool writeConfigDocumentAtomically(FS& fs, SYSTEM::SpiRamJsonDocument& doc) {
    if (!g_fsMutex) {
        LOGE("Global FS mutex is not initialized");
        return false;
    }

    SYSTEM::ScopeLock fsLock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!fsLock.isLocked()) {
        LOGW("FS mutex busy, config save skipped");
        return false;
    }

    if (!fs.exists("/config") && !fs.mkdir("/config")) {
        LOGE("Failed to create /config directory");
        return false;
    }

    File file = fs.open(kConfigTmpFile, "w");
    if (!file) {
        LOGE("Failed to open temp config file for writing");
        return false;
    }

    const size_t written = serializeJson(doc, file);
    file.close();

    if (written == 0) {
        LOGE("Failed to serialize config to JSON");
        fs.remove(kConfigTmpFile);
        return false;
    }
    if (written > kConfigFileMaxBytes) {
        LOGE("Config JSON exceeds max file size: %u bytes (max %u)",
             static_cast<unsigned>(written),
             static_cast<unsigned>(kConfigFileMaxBytes));
        fs.remove(kConfigTmpFile);
        return false;
    }
    if (doc.overflowed()) {
        LOGE("Config JSON document overflowed during save; aborting write");
        fs.remove(kConfigTmpFile);
        return false;
    }

    if (fs.exists(kConfigBakFile)) {
        fs.remove(kConfigBakFile);
    }
    if (fs.exists(kConfigFile) && !fs.rename(kConfigFile, kConfigBakFile)) {
        LOGE("Failed to backup existing config file");
        fs.remove(kConfigTmpFile);
        return false;
    }

    if (!fs.rename(kConfigTmpFile, kConfigFile)) {
        LOGE("Failed to promote temp config file");
        if (fs.exists(kConfigBakFile) && !fs.rename(kConfigBakFile, kConfigFile)) {
            LOGE("Failed to restore backup config file");
        }
        fs.remove(kConfigTmpFile);
        return false;
    }

    if (fs.exists(kConfigBakFile)) {
        fs.remove(kConfigBakFile);
    }

    LOGD("Configuration saved atomically to %s (%u bytes)",
         kConfigFile,
         static_cast<unsigned>(written));
    return true;
}

void deleteConfigFile(FS& fs) {
    if (!g_fsMutex) {
        LOGE("Global FS mutex is not initialized");
        return;
    }

    SYSTEM::ScopeLock fsLock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!fsLock.isLocked()) {
        LOGW("FS mutex busy, config delete skipped");
        return;
    }

    if (fs.exists(kConfigFile) && fs.remove(kConfigFile)) {
        LOGI("Config file deleted");
        return;
    }

    if (fs.exists(kConfigFile)) {
        LOGW("Failed to delete config file");
    }
}

}  // namespace CONFIG::Persistence
