#include "core/config/ConfigManager.h"

#include "core/config/persistence/ConfigPersistence.h"
#include "core/config/serialization/ConfigLoaders.h"
#include "core/config/serialization/ConfigSavers.h"
#include "system/logging/Logging.h"
#include "system/memory/PsramAllocator.h"

#undef LOG_TAG
#define LOG_TAG "ConfigMgr"

SemaphoreHandle_t g_fsMutex = nullptr;

namespace CONFIG {

namespace {

void setLoadFailure(LoadFailure* failure, LoadFailure value) {
    if (failure) {
        *failure = value;
    }
}

bool saveInternal(FS& fs, const ALARMS::AlarmRulesSnapshot* alarmRulesOverride) {
    SYSTEM::SpiRamJsonDocument doc(kConfigDocSize);
    Serialization::buildConfigDocument(doc, alarmRulesOverride);
    return Persistence::writeConfigDocumentAtomically(fs, doc);
}

}  // namespace

bool load(FS& fs, LoadFailure* failure) {
    setLoadFailure(failure, LoadFailure::None);
    if (!fs.exists(kConfigFile)) {
        LOGI("Config file not found, using factory defaults");
        setLoadFailure(failure, LoadFailure::NotFound);
        return false;
    }

    SYSTEM::SpiRamJsonDocument doc(kConfigDocSize);
    if (!Persistence::readConfigDocument(fs, doc, "Load")) {
        setLoadFailure(failure, LoadFailure::InvalidDocument);
        return false;
    }

    if (!Serialization::loadConfigSections(doc)) {
        LOGE("Configuration contains an invalid critical section");
        setLoadFailure(failure, LoadFailure::CriticalSection);
        return false;
    }
    LOGI("Configuration loaded from %s", kConfigFile);
    return true;
}

bool loadPsramOnly(FS& fs, LoadFailure* failure) {
    setLoadFailure(failure, LoadFailure::None);
    if (!fs.exists(kConfigFile)) {
        LOGI("PSRAM-only config hydration skipped: %s not found", kConfigFile);
        setLoadFailure(failure, LoadFailure::NotFound);
        return false;
    }

    SYSTEM::SpiRamJsonDocument doc(kConfigDocSize);
    if (!Persistence::readConfigDocument(fs, doc, "PSRAM-only load")) {
        setLoadFailure(failure, LoadFailure::InvalidDocument);
        return false;
    }

    if (!Serialization::loadPsramOnlyConfigSections(doc)) {
        LOGE("PSRAM-only configuration contains an invalid critical section");
        setLoadFailure(failure, LoadFailure::CriticalSection);
        return false;
    }
    LOGI("PSRAM-only config hydrated from %s", kConfigFile);
    return true;
}

bool save(FS& fs) {
    return saveInternal(fs, nullptr);
}

bool saveWithAlarmRules(FS& fs, const ALARMS::AlarmRulesSnapshot& alarmRules) {
    return saveInternal(fs, &alarmRules);
}

void deleteConfigFile(FS& fs) {
    Persistence::deleteConfigFile(fs);
}

}  // namespace CONFIG
