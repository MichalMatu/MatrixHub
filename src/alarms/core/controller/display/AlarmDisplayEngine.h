#pragma once

#include "../../../../system/rtc/RtcConfig.h"
#include "AlarmDisplayTypes.h"

namespace ALARMS {

class AlarmDisplayEngine {
public:
    AlarmDisplayResult build(RTC::MatrixAlarmMode mode, const AlarmDisplaySnapshot& snapshot) const;
};

}  // namespace ALARMS
