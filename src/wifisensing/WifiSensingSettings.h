/**
 * @file WifiSensingSettings.h
 * @brief Persistent settings for WiFi Sensing module - backed by RTC memory
 * 
 * Refactored 3 Jan 2026: Uses centralized ConfigManager for persistence.
 */

#pragma once

#include "../system/rtc/RtcStatefulService.h"
#include "../system/rtc/RtcConfig.h"
#include "../config/App.h"
#include "core/config/ConfigManager.h"

namespace WIFISENSING {

class WifiSensingService;
namespace CSI {
class CsiService;
}

class WifiSensingSettings : public RtcStatefulService<RTC::WifiSensingData> {
public:
    WifiSensingSettings(FS* fs,
                        WIFISENSING::WifiSensingService* service,
                        WIFISENSING::CSI::CsiService* csiService);

    void begin();
    static void readState(RTC::WifiSensingData& settings, JsonObject& root);
    static StateUpdateResult updateState(JsonObject& jsonObject, RTC::WifiSensingData& settings, std::string_view originId);

    bool isEnabled() const;
    uint16_t getSampleIntervalMs() const;
    float getVarianceThreshold() const;

private:
    StateHandlerResult onConfigUpdated();
    
    FS* _fs;
    WIFISENSING::WifiSensingService* _service; // Injected
    WIFISENSING::CSI::CsiService* _csiService; // Injected
    RTC::WifiSensingData _lastPersistedState{};
};

}  // namespace WIFISENSING
