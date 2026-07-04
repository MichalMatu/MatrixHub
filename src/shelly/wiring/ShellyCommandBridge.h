#pragma once

#include <atomic>
#include <functional>
#include <cstdint>

namespace SHELLY {
class ShellyService;
}

namespace SHELLY {

class ShellyCommandBridge {
public:
    // IMPORTANT:
    // Build with a provider that resolves ShellyService at command execution time.
    // ServiceRegistry initializes BLE core before business services, so capturing a raw
    // ShellyService* during boot can freeze nullptr and break BLE toggling.
    static std::function<bool(uint8_t, bool)> build(
        const std::atomic<bool>* isDying,
        std::function<ShellyService*()> shellyServiceProvider);
};

}  // namespace SHELLY
