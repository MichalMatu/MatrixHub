#include "AlarmShellyBridge.h"

#include "../../shelly/ShellyService.h"

namespace ALARMS {

std::function<uint8_t(const AlarmRule&, bool)> AlarmShellyBridge::build(SHELLY::ShellyService* shellyService) {
    return [shellyService](const AlarmRule& rule, bool turnOn) -> uint8_t {
        if (!shellyService || !shellyService->isRunning()) {
            return 0;
        }

        uint8_t successCount = 0;
        for (uint8_t i = 0; i < rule.shellyDeviceCount && i < kMaxShellyPerRule; i++) {
            if (rule.shellyDeviceIds[i][0] == '\0') {
                continue;
            }

            if (shellyService->setRelayState(rule.shellyDeviceIds[i], turnOn)) {
                successCount++;
            }
        }

        return successCount;
    };
}

}  // namespace ALARMS
