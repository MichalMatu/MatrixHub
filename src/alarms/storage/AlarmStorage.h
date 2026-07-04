/**
 * @file AlarmStorage.h
 * @brief PSRAM-backed persistence adapter for alarm rules
 */

#pragma once

#include <FS.h>
#include "../types/AlarmTypes.h"
#include "../../system/rtc/types/RtcAlarmTypes.h"
namespace ALARMS {

/**
 * Persists alarm rules to the config file without mutating the live rule store.
 */
class AlarmStorage {
public:
    explicit AlarmStorage(FS& fs);
    // The output snapshot buffer may live in PSRAM. This is plain config data,
    // so it does not require internal DRAM unless some caller explicitly does.
    static void buildRulesSnapshot(const AlarmRule* rules, uint8_t count, AlarmRulesSnapshot& out);
    bool persistRules(const AlarmRule* rules, uint8_t count);
    bool persistRules(const AlarmRulesSnapshot& rulesData);

private:
    FS& _fs;
};

}  // namespace ALARMS
