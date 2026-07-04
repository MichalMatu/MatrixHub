#pragma once

#include <atomic>
#include <core/ESP32SvelteKit.h>

namespace BLE {
class BleSettingsService;
class BleService;
}

/**
 * @brief BLE services initialization
 * 
 * Wires BLE settings to the already-owned BleService and configures the
 * runtime update callbacks that are safe without a full restart.
 */
class BleServicesInitializer {
public:
    /**
     * @brief Wire BLE settings and runtime-safe callbacks
     * 
     * @param bleSettings Registry-owned BLE settings service
     * @param bleService Registry-owned BLE runtime service
     * @param isDying Shutdown flag used to suppress late runtime callbacks
     * @param whitelistHandlerId Receives removable handler id for whitelist refresh
     */
    static void initialize(
        BLE::BleSettingsService* bleSettings,
        BLE::BleService* bleService,
        const std::atomic<bool>* isDying,
        std::size_t& whitelistHandlerId);
};
