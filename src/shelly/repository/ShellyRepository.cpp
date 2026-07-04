#include "ShellyRepository.h"
#include "../../system/logging/Logging.h"
#include "../ShellyConfigStore.h"
#include "../../system/rtc/RtcConfig.h"
#include "core/config/ConfigManager.h"
#include <ArduinoJson.h>

#undef LOG_TAG
#define LOG_TAG "ShellyRepo"

namespace SHELLY {

ShellyRepository::ShellyRepository(FS& fs)
    : _fs(fs) {
}

bool ShellyRepository::exists() const {
    return CONFIG_STORE::copy().deviceCount > 0;
}

bool ShellyRepository::remove() {
    CONFIG_STORE::update([](RTC::ShellyData& shelly) {
        shelly = RTC::ShellyData{};
    });
    RTC::updateConfigSection(&RTC::ConfigStore::shelly, [](RTC::ShellySummaryData& shelly) {
        shelly = RTC::ShellySummaryData{};
    });
    return CONFIG::save(_fs);
}

bool ShellyRepository::load(std::vector<ShellyDevice>& devices) {
    devices.clear();

    RTC::ShellyData shelly = CONFIG_STORE::copy();

    // Load directly from RTC (which is pre-loaded by ConfigManager)
    for (uint8_t i = 0; i < shelly.deviceCount; i++) {
        devices.push_back(shelly.devices[i]);
    }

    LOGI("Loaded %d devices from config store", devices.size());
    return true;
}

bool ShellyRepository::save(const std::vector<ShellyDevice>& devices) {
    RTC::ShellyData next{};

    for (const auto& d : devices) {
        if (next.deviceCount >= RTC::kMaxShellyDevices) {
            LOGW("Max Shelly devices reached (%d), ignoring rest", RTC::kMaxShellyDevices);
            break;
        }

        next.devices[next.deviceCount] = d;
        next.deviceCount++;
    }

    CONFIG_STORE::update([&](RTC::ShellyData& shelly) {
        shelly = next;
    });
    RTC::updateConfigSection(&RTC::ConfigStore::shelly, [&](RTC::ShellySummaryData& shelly) {
        shelly.deviceCount = next.deviceCount;
        shelly.enabledCount = 0;
        for (uint8_t i = 0; i < next.deviceCount; i++) {
            if (next.devices[i].enabled) {
                shelly.enabledCount++;
            }
        }
    });
    
    if (CONFIG::save(_fs)) {
        LOGD("Saved %d devices to config", devices.size());
        return true;
    }
    
    return false;
}

} // namespace SHELLY
