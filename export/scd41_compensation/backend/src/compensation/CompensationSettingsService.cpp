/**
 * @file CompensationSettingsService.cpp
 * @brief RTC-backed transactional settings service for compensation config
 */

#include "CompensationSettingsService.h"

#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"
#include "CompensationService.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "CompSettings"

namespace COMPENSATION {

CompensationSettingsService::CompensationSettingsService(FS* fs, CompensationService* service)
    : RtcStatefulService(&RTC::ConfigStore::compensation),
      _fs(fs),
      _service(service) {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistAndApply();
        },
        false);
}

void CompensationSettingsService::readState(RTC::CompensationData& settings, JsonObject& root) {
    root[CONFIG::Keys::kEnabled] = settings.enabled;
    root[CONFIG::Keys::kBaseTempOffset] = settings.baseTempOffset;
    root[CONFIG::Keys::kReferenceCpuTemp] = settings.referenceCpuTemp;
    root[CONFIG::Keys::kTempOffsetPerCpuDegree] = settings.tempOffsetPerCpuDegree;
    root[CONFIG::Keys::kMinTempOffset] = settings.minTempOffset;
    root[CONFIG::Keys::kMaxTempOffset] = settings.maxTempOffset;
}

StateUpdateResult CompensationSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::CompensationData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::CompensationData nextState = settings;
    CONFIG::JSON::deserializeCompensation(jsonObject, nextState);

    if (memcmp(&settings, &nextState, sizeof(RTC::CompensationData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult CompensationSettingsService::persistAndApply() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist compensation settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    if (_service) {
        _service->applySettings();
    }

    return StateHandlerResult::success();
}

}  // namespace COMPENSATION
