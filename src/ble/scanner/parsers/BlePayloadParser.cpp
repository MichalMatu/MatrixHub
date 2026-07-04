#include "BlePayloadParser.h"
#include <cstring>

namespace BLE {

bool BlePayloadParser::payloadHasNamePrefix(const uint8_t* payload, size_t length, const char* prefix, size_t prefixLen, bool* outHasNameField) {
    if (!payload || length == 0 || !prefix || prefixLen == 0) return false;
    if (outHasNameField) *outHasNameField = false;

    size_t i = 0;
    while (i < length) {
        const uint8_t fieldLen = payload[i];
        if (fieldLen == 0) {
            break;
        }

        const size_t fieldStart = i + 1;
        const size_t fieldEnd = fieldStart + fieldLen;
        if (fieldEnd > length) {
            break;  // Malformed payload
        }

        if (fieldLen >= 2) {
            const uint8_t adType = payload[fieldStart];
            const uint8_t* data = payload + fieldStart + 1;
            const size_t dataLen = fieldLen - 1;

            // 0x08: Shortened Local Name, 0x09: Complete Local Name
            if ((adType == 0x08 || adType == 0x09)) {
                if (outHasNameField) *outHasNameField = true;
                
                // Only check match if data is long enough
                if (dataLen >= prefixLen) {
                    if (memcmp(data, prefix, prefixLen) == 0) {
                        return true;
                    }
                }
            }
        }

        i = fieldEnd;
    }

    return false;
}


const uint8_t* BlePayloadParser::findServiceData(const uint8_t* payload, size_t length, uint16_t uuid, size_t* outLen) {
    if (!payload || length == 0) return nullptr;

    size_t i = 0;
    while (i < length) {
        uint8_t len = payload[i];
        if (len == 0) break;
        
        if (i + 1 + len > length) break; // Malformed
        
        uint8_t type = payload[i+1];
        
        // 0x16: Service Data - 16-bit UUID
        if (type == 0x16) { 
             if (len < 3) { i += 1 + len; continue; } // Need at least UUID (2 bytes)
             
             // UUID is Little Endian in AD
             uint16_t foundUuid = payload[i+2] | (payload[i+3] << 8);
             
             if (foundUuid == uuid) {
                 if (outLen) *outLen = len - 3; // Minus type(1) and UUID(2)
                 // Data starts after UUID (type + 2 bytes UUID = 3 bytes offset from type field)
                 // Type is at i+1, UUID at i+2..i+3, Data at i+4
                 return &payload[i+4];
             }
        }
        i += 1 + len;
    }
    return nullptr;
}


} // namespace BLE
