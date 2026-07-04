/**
 * @file AlarmSettingsPayload.cpp
 * @brief Stateless helpers for alarm settings payload validation and mapping
 */

#include "AlarmSettingsPayload.h"

#include "../config/App.h"
#include "../config/json/AlarmConfigJson.h"
#include "../system/logging/Logging.h"
#include "../system/memory/SystemAllocator.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "AlarmSettingsPayload"

namespace ALARMS::AlarmSettingsPayload {

bool areRulesEqual(const AlarmRulesSnapshot& lhs, const AlarmRulesSnapshot& rhs) {
    if (lhs.ruleCount != rhs.ruleCount) {
        return false;
    }

    const size_t bytes = sizeof(ALARMS::AlarmRule) * lhs.ruleCount;
    return bytes == 0 || memcmp(lhs.rules, rhs.rules, bytes) == 0;
}

StateHandlerResult parseUpdate(JsonObject& jsonObject, AlarmRulesSnapshot* parsedState) {
    const JsonObjectConst input = jsonObject;
    const int schemaVersion = input[CONFIG::Keys::kSchemaVersion] | 0;
    if (schemaVersion != 1) {
        LOGW("parseUpdate: invalid schema version %d", schemaVersion);
        return StateHandlerResult::failure("input/alarm_rules_invalid_schema", 400);
    }

    JsonVariantConst rulesVariant = input[CONFIG::Keys::kRules];
    if (!rulesVariant.is<JsonArrayConst>()) {
        LOGW("parseUpdate: missing rules array");
        return StateHandlerResult::failure("input/alarm_rules_missing_rules", 400);
    }

    auto parsed = SYSTEM::MEMORY::makeUniqueInPsram<AlarmRulesSnapshot>();
    if (!parsed) {
        LOGE("parseUpdate: failed to allocate parsed alarm rules in PSRAM");
        return StateHandlerResult::failure("internal/update_failed");
    }

    JsonArray rulesArray = jsonObject[CONFIG::Keys::kRules].as<JsonArray>();
    CONFIG::JSON::AlarmRulesParseError parseError = CONFIG::JSON::AlarmRulesParseError::None;
    if (!CONFIG::JSON::deserializeAlarmRules(rulesArray, *parsed, &parseError)) {
        if (parseError == CONFIG::JSON::AlarmRulesParseError::DuplicateRuleId) {
            LOGW("parseUpdate: duplicate alarm rule id");
            return StateHandlerResult::failure("input/alarm_rules_duplicate_id", 400);
        }
        if (parseError == CONFIG::JSON::AlarmRulesParseError::DuplicateRuleName) {
            LOGW("parseUpdate: duplicate alarm rule name");
            return StateHandlerResult::failure("input/alarm_rules_duplicate_name", 400);
        }
        if (parseError == CONFIG::JSON::AlarmRulesParseError::TooManyRules) {
            LOGW("parseUpdate: too many alarm rules");
            return StateHandlerResult::failure("input/alarm_rules_too_many", 400);
        }
        LOGW("parseUpdate: invalid alarm rule data");
        return StateHandlerResult::failure("input/alarm_rules_invalid_rule", 400);
    }

    if (parsedState) {
        *parsedState = *parsed;
    }

    return StateHandlerResult::success();
}

void readState(AlarmRulesSnapshot& settings, JsonObject& root) {
    root[CONFIG::Keys::kSchemaVersion] = 1;
    CONFIG::JSON::saveAlarms(root, settings);
}

StateUpdateResult updateState(
    JsonObject& jsonObject,
    AlarmRulesSnapshot& settings,
    std::string_view originId) {
    (void)originId;

    auto nextState = SYSTEM::MEMORY::makeUniqueInPsram<AlarmRulesSnapshot>();
    if (!nextState) {
        LOGE("updateState: failed to allocate next alarm rules in PSRAM");
        return StateUpdateResult::ERROR;
    }

    const StateHandlerResult validation = parseUpdate(jsonObject, nextState.get());
    if (!validation.ok) {
        return StateUpdateResult::ERROR;
    }

    if (areRulesEqual(settings, *nextState)) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = *nextState;
    return StateUpdateResult::CHANGED;
}

}  // namespace ALARMS::AlarmSettingsPayload
