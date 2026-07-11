#include "AlarmShellyBridge.h"

#include "../../shelly/ShellyService.h"

namespace ALARMS {

ShellyActionExecutor AlarmShellyBridge::build(SHELLY::ShellyService* shellyService) {
    return [shellyService](const AlarmRule& rule, bool turnOn) -> ShellyActionResult {
        if (!shellyService) {
            return ShellyActionResult::Retry;
        }

        bool accepted = false;
        for (uint8_t i = 0; i < rule.shellyDeviceCount && i < kMaxShellyPerRule; i++) {
            if (rule.shellyDeviceIds[i][0] == '\0') {
                continue;
            }

            const SHELLY::ShellyRelayAdmissionResult result =
                shellyService->trySetAlarmRelayState(
                    rule.shellyDeviceIds[i], turnOn);
            if (result == SHELLY::ShellyRelayAdmissionResult::Retry) {
                return ShellyActionResult::Retry;
            }
            if (result == SHELLY::ShellyRelayAdmissionResult::Accepted) {
                accepted = true;
            }
        }

        return accepted ? ShellyActionResult::Accepted
                        : ShellyActionResult::Terminal;
    };
}

}  // namespace ALARMS
