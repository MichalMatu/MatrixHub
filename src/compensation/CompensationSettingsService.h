/**
 * @file CompensationSettingsService.h
 * @brief RTC-backed transactional settings service for compensation config
 */

#pragma once

#include <FS.h>

#include "../config/App.h"
#include "../config/json/CompensationConfigJson.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/rtc/RtcStatefulService.h"

namespace COMPENSATION {

class CompensationService;

class CompensationSettingsService : public RtcStatefulService<RTC::CompensationData> {
public:
    CompensationSettingsService(FS* fs, CompensationService* service);

    static void readState(RTC::CompensationData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::CompensationData& settings,
        std::string_view originId);

private:
    StateHandlerResult persistAndApply();

    FS* _fs;
    CompensationService* _service;
};

}  // namespace COMPENSATION
