#pragma once

#include "../../../alarms/types/AlarmTypes.h"
#include "../../../system/utils/json/JsonResponseWriter.h"

namespace API {

struct RuleStatus {
    bool triggered = false;
    uint32_t lastTriggered = 0;
    float currentValue = NAN;
    bool valid = false;
};

class AlarmRulesSerializer {
public:
    /**
     * @brief Serialize alarm rules list to JSON
     * 
     * @param w JSON writer
     * @param rules Array of rules
     * @param count Number of rules
     * @param statuses Optional parallel status array indexed like rules (can be null)
     * @param includeStatus Whether to include runtime status fields
     * @return true on success
     */
    static bool serialize(Utils::JsonResponseWriter& w, 
                         const ALARMS::AlarmRule* rules, 
                         uint8_t count,
                         const RuleStatus* statuses,
                         bool includeStatus);
};

} // namespace API
