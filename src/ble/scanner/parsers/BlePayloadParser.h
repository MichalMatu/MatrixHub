#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace BLE {

class BlePayloadParser {
public:
    /**
     * Checks if the payload contains a Local Name (Complete 0x09 or Shortened 0x08)
     * matching the given prefix.
     * 
     * @param payload Raw BLE AD payload
     * @param length Length of the payload
     * @param prefix Prefix string to match (e.g., "TP357")
     * @param prefixLen Length of the prefix
     * @param outHasNameField Optional output pointer, set to true if ANY name field is found (even if prefix doesn't match)
     * @return true if a name field exists and matches the prefix
     */
    static bool payloadHasNamePrefix(const uint8_t* payload, size_t length, const char* prefix, size_t prefixLen, bool* outHasNameField = nullptr);


    /**
     * Finds Service Data (0x16) for a specific 16-bit UUID.
     * 
     * @param payload Raw BLE AD payload
     * @param length Length of the payload
     * @param uuid 16-bit UUID to search for (e.g., 0xFCD2 for BTHome)
     * @param outLen Output pointer for the length of the found data (excluding UUID)
     * @return Pointer to the data payload (starting after the UUID), or nullptr if not found
     */
    static const uint8_t* findServiceData(const uint8_t* payload, size_t length, uint16_t uuid, size_t* outLen);

};

} // namespace BLE
