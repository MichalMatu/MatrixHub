#pragma once

#include "../../../../system/matrix_manager/MatrixManagerTypes.h"
#include "../../../types/AlarmConstants.h"
#include "../../../types/AlarmEnums.h"

namespace ALARMS {

struct AlarmDisplaySnapshot {
    bool active = false;
    AlarmSeverity severity = AlarmSeverity::Info;
    const char* alarmName = nullptr;
};

struct AlarmDisplayResult {
    bool clearLayer = true;
    MATRIX_MANAGER::LayerContent content{};
};

}  // namespace ALARMS
