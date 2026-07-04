/**
 * @file AlarmSettingsPayload.h
 * @brief Stateless helpers for alarm settings payload validation and mapping
 */

#pragma once

#include <ArduinoJson.h>

#include <core/StatefulService.h>

#include "../system/rtc/types/RtcAlarmTypes.h"

namespace ALARMS::AlarmSettingsPayload {

bool areRulesEqual(const AlarmRulesSnapshot& lhs, const AlarmRulesSnapshot& rhs);
StateHandlerResult parseUpdate(JsonObject& jsonObject, AlarmRulesSnapshot* parsedState = nullptr);
void readState(AlarmRulesSnapshot& settings, JsonObject& root);
StateUpdateResult updateState(
    JsonObject& jsonObject,
    AlarmRulesSnapshot& settings,
    std::string_view originId);

}  // namespace ALARMS::AlarmSettingsPayload
