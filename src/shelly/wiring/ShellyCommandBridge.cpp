#include "ShellyCommandBridge.h"

#include "../ShellyConfigStore.h"
#include "../ShellyService.h"
#include <cstring>
#include <utility>

namespace SHELLY {

std::function<bool(uint8_t, bool)> ShellyCommandBridge::build(
    const std::atomic<bool>* isDying,
    std::function<ShellyService*()> shellyServiceProvider) {
    return [isDying, shellyServiceProvider = std::move(shellyServiceProvider)](uint8_t deviceIndex, bool turnOn) -> bool {
        if (isDying && isDying->load(std::memory_order_acquire)) {
            return false;
        }

        if (deviceIndex >= RTC::kMaxShellyDevices) {
            return false;
        }

        ShellyService* shellyService = shellyServiceProvider ? shellyServiceProvider() : nullptr;
        if (!shellyService) {
            return false;
        }

        char deviceId[SHELLY::kMaxShellyId] = {0};
        bool valid = false;

        CONFIG_STORE::withConfig([&](const RTC::ShellyData& shellyData) {
            const auto& device = shellyData.devices[deviceIndex];
            if (device.isValid()) {
                strncpy(deviceId, device.id, sizeof(deviceId) - 1);
                deviceId[sizeof(deviceId) - 1] = '\0';
                valid = true;
            }
        });

        if (!valid || deviceId[0] == '\0') {
            return false;
        }

        return shellyService->setRelayState(deviceId, turnOn);
    };
}

}  // namespace SHELLY
