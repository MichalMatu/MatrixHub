#pragma once
#include <ArduinoJson.h>
#include "../../alarms/types/AlarmRule.h"
#include "../../system/rtc/types/RtcAlarmTypes.h"

namespace CONFIG {
namespace JSON {

	    enum class AlarmRulesParseError : uint8_t {
	        None = 0,
	        InvalidRuleData,
	        DuplicateRuleId,
	        DuplicateRuleName,
	        TooManyRules
	    };

	    void loadAlarms(JsonObject& obj);
	    void saveAlarms(JsonObject& obj);
	    void saveAlarms(JsonObject& obj, const ALARMS::AlarmRulesSnapshot& rules);
	    
	    // Shared deserialization logic for Config loading and API PUT requests
	    // Returns false if rule violates length/range validation
	    bool deserializeAlarmRule(JsonObject& rule, ALARMS::AlarmRule& r);
	    bool deserializeAlarmRules(
	        JsonArray& rules,
	        ALARMS::AlarmRulesSnapshot& parsed,
	        AlarmRulesParseError* error = nullptr
	    );

} // namespace JSON
} // namespace CONFIG
