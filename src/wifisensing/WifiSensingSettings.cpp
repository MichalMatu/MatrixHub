/**
 * @file WifiSensingSettings.cpp
 * @brief WiFi Sensing settings service - backed by RTC memory
 * 
 * Refactored 3 Jan 2026: Uses centralized ConfigManager for persistence.
 */

#include "WifiSensingSettings.h"
#include "WifiSensingService.h"
#include "csi/core/CsiService.h"
#include "../config/json/WifiSensingConfigJson.h"
#include "../system/logging/Logging.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "WifiSenseSettings"

namespace WIFISENSING {
namespace {

CSI::CsiMotionConfig toCsiMotionConfig(const RTC::WifiSensingData& state) {
    CSI::CsiMotionConfig config;
    config.enabled = state.csiAlarmEnabled;
    config.bandCount = state.csiAlarmBandCount > CSI::MAX_CSI_ALARM_BANDS
                           ? CSI::MAX_CSI_ALARM_BANDS
                           : state.csiAlarmBandCount;
    for (uint8_t i = 0; i < config.bandCount; ++i) {
        config.bands[i].start = state.csiAlarmBandStart[i];
        config.bands[i].end = state.csiAlarmBandEnd[i];
    }
    config.baselineFrames = state.csiBaselineFrames;
    config.topK = state.csiTopK;
    config.enterThreshold = state.csiEnterThreshold;
    config.clearThreshold = state.csiClearThreshold;
    config.holdMs = state.csiHoldMs;
    config.clearHoldMs = state.csiClearHoldMs;
    config.minNoise = state.csiMinNoise;
    config.minEnergy = state.csiMinEnergy;
    config.noisyScoreThreshold = state.csiNoisyThreshold;
    config.autoRecalibration = state.csiAutoRecalibration;
    config.sensitivity = state.csiSensitivity;
    return config;
}

bool csiAlarmConfigChanged(const RTC::WifiSensingData& a, const RTC::WifiSensingData& b) {
    if (a.csiAlarmEnabled != b.csiAlarmEnabled ||
        a.csiAlarmBandCount != b.csiAlarmBandCount ||
        a.csiBaselineFrames != b.csiBaselineFrames ||
        a.csiTopK != b.csiTopK ||
        a.csiEnterThreshold != b.csiEnterThreshold ||
        a.csiClearThreshold != b.csiClearThreshold ||
        a.csiHoldMs != b.csiHoldMs ||
        a.csiClearHoldMs != b.csiClearHoldMs ||
        a.csiMinNoise != b.csiMinNoise ||
        a.csiMinEnergy != b.csiMinEnergy ||
        a.csiNoisyThreshold != b.csiNoisyThreshold ||
        a.csiAutoRecalibration != b.csiAutoRecalibration ||
        a.csiSensitivity != b.csiSensitivity) {
        return true;
    }

    for (uint8_t i = 0; i < 4; ++i) {
        if (a.csiAlarmBandStart[i] != b.csiAlarmBandStart[i] ||
            a.csiAlarmBandEnd[i] != b.csiAlarmBandEnd[i]) {
            return true;
        }
    }

    return false;
}

} // namespace

WifiSensingSettings::WifiSensingSettings(FS* fs,
                                         WIFISENSING::WifiSensingService* service,
                                         WIFISENSING::CSI::CsiService* csiService)
    : RtcStatefulService(&RTC::ConfigStore::wifiSensing),
      _fs(fs),
      _service(service),
      _csiService(csiService) {
    
    addUpdateHandler([this](std::string_view originId) { 
        (void)originId;
        return onConfigUpdated();
    }, false);
}

void WifiSensingSettings::begin() {
    _lastPersistedState = _state;
    
    // Data is already in RTC
    LOGI("Settings (RTC): enabled=%d, interval=%ums, threshold=%.2f, csi_alarm=%d",
         _state.enabled ? 1 : 0,
         _state.sampleIntervalMs,
         _state.varianceThreshold,
         _state.csiAlarmEnabled ? 1 : 0);
    
    // Start service if enabled
    if (_state.enabled && _service) {
        if (!_service->begin(_state.sampleIntervalMs, _state.varianceThreshold)) {
            LOGE("Failed to apply persisted WiFi sensing state during boot");
        }
    }

    if (_csiService) {
        _csiService->setMotionConfig(toCsiMotionConfig(_state));
    }
}

void WifiSensingSettings::readState(RTC::WifiSensingData& settings, JsonObject& root) {
    CONFIG::JSON::serializeWifiSensing(root, settings);
}

StateUpdateResult WifiSensingSettings::updateState(
    JsonObject& jsonObject,
    RTC::WifiSensingData& settings,
    std::string_view originId) {
    (void)originId;
    RTC::WifiSensingData nextState = settings;
    CONFIG::JSON::deserializeWifiSensing(jsonObject, nextState);

    if (memcmp(&settings, &nextState, sizeof(RTC::WifiSensingData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult WifiSensingSettings::onConfigUpdated() {
    const RTC::WifiSensingData previousState = _lastPersistedState;
    const RTC::WifiSensingData nextState = _state;

    const auto applyRuntimeState = [this](const RTC::WifiSensingData& fromState,
                                          const RTC::WifiSensingData& toState) -> bool {
        if (_service) {
            if (fromState.enabled != toState.enabled) {
                if (toState.enabled) {
                    LOGI("Enabling WiFi Sensing...");
                    if (!_service->begin(toState.sampleIntervalMs, toState.varianceThreshold)) {
                        return false;
                    }
                } else {
                    LOGI("Disabling WiFi Sensing...");
                    if (!_service->stop()) {
                        return false;
                    }
                }
            } else if (toState.enabled) {
                // Reconfigure the live worker in place. If runtime drift already left
                // the service stopped, start it fresh instead of reporting a false
                // "saved" success while sensing stays dead until reboot.
                LOGI("Updating WiFi Sensing settings...");
                if (!_service->isRunning()) {
                    if (!_service->begin(toState.sampleIntervalMs, toState.varianceThreshold)) {
                        return false;
                    }
                } else if (!_service->stop() ||
                           !_service->begin(toState.sampleIntervalMs, toState.varianceThreshold)) {
                    return false;
                }
            }
        }

        if (_csiService && csiAlarmConfigChanged(fromState, toState)) {
            _csiService->setMotionConfig(toCsiMotionConfig(toState));
        }

        return true;
    };

    LOGI("Settings updated: enabled=%d, interval=%ums, threshold=%.2f, csi_alarm=%d",
         nextState.enabled ? 1 : 0,
         nextState.sampleIntervalMs,
         nextState.varianceThreshold,
         nextState.csiAlarmEnabled ? 1 : 0);

    // Apply runtime first. If that fails, the outer transactional RTC service
    // can still roll the in-memory/RTC state back without leaving the live
    // worker silently diverged from the requested config.
    if (!applyRuntimeState(previousState, nextState)) {
        LOGE("Failed to apply WiFi sensing runtime state");
        return StateHandlerResult::failure("config/apply_failed");
    }

    // Persist only after runtime accepted the transition. If the filesystem
    // save fails, revert the live runtime back to the last known persisted
    // state so RTC rollback and runtime behavior stay aligned.
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist WiFi sensing settings");
        if (!applyRuntimeState(nextState, previousState)) {
            LOGE("Failed to roll back WiFi sensing runtime after save failure");
        }
        return StateHandlerResult::failure("config/save_failed");
    }

    _lastPersistedState = nextState;
    return StateHandlerResult::success();
}

bool WifiSensingSettings::isEnabled() const {
    return _state.enabled;
}

uint16_t WifiSensingSettings::getSampleIntervalMs() const {
    return _state.sampleIntervalMs;
}

float WifiSensingSettings::getVarianceThreshold() const {
    return _state.varianceThreshold;
}

}  // namespace WIFISENSING
