#pragma once

#include "../types/AlarmRule.h"
#include "../types/AlarmConstants.h"
#include "../core/AlarmCoordinator.h"

namespace SHELLY {
class ShellyService;
}

namespace ALARMS {

class AlarmShellyBridge {
public:
    static ShellyActionExecutor build(SHELLY::ShellyService* shellyService);
};

}  // namespace ALARMS
