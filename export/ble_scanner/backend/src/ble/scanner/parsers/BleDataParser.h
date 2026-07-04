#pragma once

#include "../filters/BleDeviceTypeDetector.h"
#include "TpParser.h" // For TpData struct

// Forward declaration (avoid NimBLE include in header for testability)
class NimBLEAdvertisedDevice;

namespace RTC {
struct RtcBleStats; // Forward declaration
}

namespace BLE {

/**
 * @brief Parses BLE advertising data into unified TpData format.
 * 
 * Supports TP357 (via manufacturer data) and BTHome (via service UUID 0xFCD2).
 * Delegates to TpParser and BtHomeParser but provides unified interface.
 */
class BleDataParser {
public:
    /**
     * @brief Parse device advertising data based on detected type.
     * 
     * @param device NimBLE advertised device
     * @param detection Device type detection result (with BTHome data pointer if applicable)
     * @param telemetry RTC telemetry counters to update (advHasMfgData, advBtHomeData)
     * @param outEncrypted [out] Set to true if BTHome encrypted packet detected
     * @return Parsed TpData (valid=true if successfully parsed)
     */
    static TpData parse(
        const NimBLEAdvertisedDevice* device,
        const BleDeviceTypeResult& detection,
        RTC::RtcBleStats& telemetry,
        bool* outEncrypted = nullptr
    );
};

} // namespace BLE
