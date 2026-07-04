#pragma once

#include "../types/AlarmRule.h"
#include "../types/AlarmConstants.h"
#include <functional>

namespace SHELLY {
class ShellyService;
}

namespace ALARMS {

class AlarmShellyBridge {
public:
    static std::function<uint8_t(const AlarmRule&, bool)> build(SHELLY::ShellyService* shellyService);
};

}  // namespace ALARMS
