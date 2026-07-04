#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../ShellyTypes.h"

namespace SHELLY {

/**
 * @brief Pure protocol logic for Shelly Gen 1 devices
 * 
 * Handles:
 * - URL generation for API calls
 * - JSON response parsing
 * 
 * Thread-safe and heap-efficient (uses stack buffers).
 */
class ShellyProtocol {
public:
    // ========================================================================
    // Gen 1 (Shelly 1, 1PM, 2.5, etc.)
    // ========================================================================

    /**
     * Build URL for controlling a relay (Gen 1)
     * Format: http://<ip>/relay/<index>?turn=<on|off>
     */
    static bool buildGen1ControlUrl(char* buffer, size_t size, const char* ip, uint8_t relayIndex, bool turnOn);

    /**
     * Build URL for querying device status (Gen 1)
     * Format: http://<ip>/status
     */
    static bool buildGen1StatusUrl(char* buffer, size_t size, const char* ip);

    /**
     * Parse Shelly status JSON (Gen 1).
     */
    static bool parseGen1Status(char* jsonResponse, size_t length, uint8_t relayIndex, ShellyStatus& outStatus);


    // ========================================================================
    // Gen 2/3 (Plus, Pro, Mini, etc.)
    // ========================================================================

    /**
     * Build URL for controlling a relay (Gen 2 RPC)
     * Format: http://<ip>/rpc/Switch.Set?id=<index>&on=<true|false>
     */
    static bool buildGen2ControlUrl(char* buffer, size_t size, const char* ip, uint8_t relayIndex, bool turnOn);

    /**
     * Build URL for querying device status (Gen 2 RPC)
     * Format: http://<ip>/rpc/Shelly.GetStatus
     */
    static bool buildGen2StatusUrl(char* buffer, size_t size, const char* ip);

    /**
     * Parse Gen 2/3 Status JSON (RPC).
     */
    static bool parseGen2Status(char* jsonResponse, size_t length, uint8_t relayIndex, ShellyStatus& outStatus);
};

} // namespace SHELLY
