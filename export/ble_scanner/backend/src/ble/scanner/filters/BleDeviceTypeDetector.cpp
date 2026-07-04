#include "BleDeviceTypeDetector.h"
#include "../parsers/BlePayloadParser.h"
#include "../parsers/BtHomeParser.h" // For BTHOME_SERVICE_UUID constant

namespace BLE {

BleDeviceTypeResult BleDeviceTypeDetector::detect(const uint8_t* payload, size_t payloadLen) {
    BleDeviceTypeResult result;
    
    if (!payload || payloadLen == 0) {
        return result; // Unknown
    }
    
    // Check 1: Name prefix (TP357, ATC, LYWSD)
    bool hasNameField = false;
    
    if (BlePayloadParser::payloadHasNamePrefix(payload, payloadLen, "TP357", 5, &hasNameField)) {
        result.type = BleDeviceType::TP357;
        return result;
    }
    
    if (BlePayloadParser::payloadHasNamePrefix(payload, payloadLen, "ATC", 3, &hasNameField) ||
        BlePayloadParser::payloadHasNamePrefix(payload, payloadLen, "LYWSD", 5, &hasNameField)) {
        result.type = BleDeviceType::BTHome;
        // Don't return yet - we need to find BTHome service data
    }
    
    // Check 2: If no name match or BTHome prefix matched, locate BTHome Service UUID 0xFCD2 data
    // Use raw payload scan to avoid heap allocation from getServiceData()
    
    if (result.type == BleDeviceType::Unknown || result.type == BleDeviceType::BTHome) {
        size_t btHomeDataLen = 0;
        const uint8_t* btHomeData = BlePayloadParser::findServiceData(payload, payloadLen, BTHOME_SERVICE_UUID, &btHomeDataLen);
        if (btHomeData) {
            result.type = BleDeviceType::BTHome;
            result.btHomeData = btHomeData;
            result.btHomeDataLen = btHomeDataLen;
        }
    }
    
    return result;
}

} // namespace BLE
