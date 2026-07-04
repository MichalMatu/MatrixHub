#pragma once

#include <functional>

#include "../system/rtc/types/RtcAlarmTypes.h"

namespace ALARMS {
namespace RULES_CONFIG {

// Caller-provided snapshot buffer may live in PSRAM. These rule snapshots are
// plain CPU-owned config data, so they do not need internal DRAM.
//
// Before the refactor this API returned AlarmRulesData by value, which created
// another ~2 KB object on the caller stack. copyTo(...) keeps the copy explicit
// and lets the caller choose PSRAM as the destination.
bool copyTo(AlarmRulesSnapshot& out);
void withRules(const std::function<void(const AlarmRulesSnapshot&)>& reader);
bool update(const std::function<void(AlarmRulesSnapshot&)>& updater);

}  // namespace RULES_CONFIG
}  // namespace ALARMS
