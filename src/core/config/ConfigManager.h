#pragma once

#include "core/config/ConfigCommon.h"
#include "alarms/types/AlarmRule.h"
#include "system/rtc/types/RtcAlarmTypes.h"

namespace CONFIG {

bool load(FS& fs);
bool loadPsramOnly(FS& fs);
bool save(FS& fs);
bool saveWithAlarmRules(FS& fs, const ALARMS::AlarmRulesSnapshot& alarmRules);
void deleteConfigFile(FS& fs);

}  // namespace CONFIG
