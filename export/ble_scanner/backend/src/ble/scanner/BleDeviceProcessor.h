#pragma once

#include <NimBLEDevice.h>
#include "filters/BleDeviceWhitelist.h"
#include "../BleTypes.h"

namespace RTC {
struct RtcBleStats; // Forward declaration
}

namespace BLE {
class BleScanner; // Forward declaration

/**
 * @brief Logic for processing a single scanned BLE device
 * 
 * Extracted from BleScanner to improve readability and testability.
 * Handles:
 * - Telemetry updates
 * - Device type detection (TP357 / BTHome)
 * - Whitelist filtering
 * - Data parsing
 * - Cache updating and callback triggering
 */
class BleDeviceProcessor {
public:
    /**
     * @brief Process a discovered device
     * 
     * @param device The NimBLE device advertisement
     * @param scanner Reference to scanner (for cache/storage)
     * @param whitelist Reference to whitelist manager
     * @param callback Callback for valid data
     * @param discoveryMode Whether discovery mode is active (accept all)
     * @param targetedMode Whether targeted mode is active (skip non-whitelisted)
     */
    static bool process(
        const NimBLEAdvertisedDevice* device,
        BleScanner& scanner,
        BleDeviceWhitelist& whitelist,
        TpDataCallback callback,
        bool discoveryMode,
        bool targetedMode,
        RTC::RtcBleStats& telemetry
    );
};

} // namespace BLE
