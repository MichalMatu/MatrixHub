#include "BleDataParser.h"
#include "../../../system/rtc/types/RtcRuntimeTypes.h"
#include "TpParser.h"
#include "BtHomeParser.h"
#include <NimBLEAdvertisedDevice.h> // Implementation needs full definition

namespace BLE {

TpData BleDataParser::parse(
    const NimBLEAdvertisedDevice* device,
    const BleDeviceTypeResult& detection,
    RTC::RtcBleStats& telemetry,
    bool* outEncrypted
) {
    TpData data = {0, 0, 0, false};
    
    if (!device) {
        return data;
    }
    
    if (outEncrypted) {
        *outEncrypted = false;
    }
    
    if (detection.type == BleDeviceType::TP357) {
        // TP357: Extract from Manufacturer Data
        size_t mfgLen = 0;
        const uint8_t* mfgData = device->getManufacturerDataRaw(0, &mfgLen);
        
        if (mfgData && mfgLen > 0) {
            telemetry.advHasMfgData++;
            data = TpParser::parse(mfgData, mfgLen);
        }
    } 
    else if (detection.type == BleDeviceType::BTHome) {
        // BTHome: Parse from detection result pointer (no copy/allocation)
        if (detection.btHomeData && detection.btHomeDataLen > 0) {
            telemetry.advBtHomeData++;
            BtHomeData btData = BtHomeParser::parse(detection.btHomeData, detection.btHomeDataLen);
            
            if (btData.valid) {
                // Convert BtHomeData to TpData for unified handling
                data.temperature = btData.temperature;
                data.humidity = btData.humidity;
                data.battery = btData.hasBattery ? btData.battery : 0;
                data.valid = true;
            } else if (btData.encrypted && outEncrypted) {
                *outEncrypted = true;
            }
        }
    }
    
    return data;
}

} // namespace BLE
