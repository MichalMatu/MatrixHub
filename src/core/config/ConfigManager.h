#pragma once

#include "core/config/ConfigCommon.h"
#include "alarms/types/AlarmRule.h"
#include "system/rtc/types/RtcAlarmTypes.h"

#include <cstdint>

namespace CONFIG {

enum class LoadFailure : uint8_t {
    None = 0,
    NotFound,
    InvalidDocument,
    CriticalSection,
};

bool load(FS& fs, LoadFailure* failure = nullptr);
bool loadPsramOnly(FS& fs, LoadFailure* failure = nullptr);
bool save(FS& fs);
bool saveWithAlarmRules(FS& fs, const ALARMS::AlarmRulesSnapshot& alarmRules);
void deleteConfigFile(FS& fs);

}  // namespace CONFIG
