/**
 * @file BleDeviceProcessor.cpp
 * @brief BLE device processing logic
 */

#include "BleDeviceProcessor.h"
#include "BleScanner.h" 
#include "parsers/BlePayloadParser.h"
#include "filters/BleDeviceTypeDetector.h"
#include "parsers/BleDataParser.h"
#include "../../system/logging/Logging.h"
#include "../../system/rtc/types/RtcRuntimeTypes.h" // For RtcBleStats (DI parameter type)

#undef LOG_TAG
#define LOG_TAG "BleProc"

namespace BLE {

bool BleDeviceProcessor::process(
    const NimBLEAdvertisedDevice* device,
    BleScanner& scanner,
    BleDeviceWhitelist& whitelist,
    TpDataCallback callback,
    bool discoveryMode,
    bool targetedMode,
    RTC::RtcBleStats& telemetry
) {
    telemetry.advTotal++;

    // Periodic telemetry (keep lightweight; no heap)
    const uint32_t now = millis();
    if (telemetry.lastLogMs == 0) {
        telemetry.lastLogMs = now;
    }
    if (now - telemetry.lastLogMs >= 60000) {
        const uint32_t dAdv = telemetry.advTotal - telemetry.lastAdvTotal;
        const uint32_t dMatch = telemetry.advMatchedNamePrefix - telemetry.lastAdvMatchedNamePrefix;
        const uint32_t dValid = telemetry.advParsedValid - telemetry.lastAdvParsedValid;
        const uint32_t dCb = telemetry.advCallback - telemetry.lastAdvCallback;

        LOGD("BLE telem (60s): adv=%u match=%u valid=%u cb=%u | totals: adv=%u match=%u tp357=%u bthome=%u valid=%u wl=%u cb=%u parserErr=%u cacheDrop=%u mutexTo=%u",
             (unsigned)dAdv, (unsigned)dMatch, (unsigned)dValid, (unsigned)dCb,
             (unsigned)telemetry.advTotal,
             (unsigned)telemetry.advMatchedNamePrefix,
             (unsigned)telemetry.advHasMfgData,
             (unsigned)telemetry.advBtHomeData,
             (unsigned)telemetry.advParsedValid,
             (unsigned)telemetry.advWhitelisted,
             (unsigned)telemetry.advCallback,
             (unsigned)telemetry.parserErrors,
             (unsigned)telemetry.cacheDrops,
             (unsigned)telemetry.mutexTimeouts);

        telemetry.lastLogMs = now;
        telemetry.lastAdvTotal = telemetry.advTotal;
        telemetry.lastAdvMatchedNamePrefix = telemetry.advMatchedNamePrefix;
        telemetry.lastAdvParsedValid = telemetry.advParsedValid;
        telemetry.lastAdvCallback = telemetry.advCallback;
    }

    // Detect device type (TP357 or BTHome)
    const auto& payload = device->getPayload();
    BleDeviceTypeResult detection = BleDeviceTypeDetector::detect(payload.data(), payload.size());
    
    // Skip unknown devices early
    if (detection.type == BleDeviceType::Unknown) {
        // LOGV("Skipping unknown device: %s", device->getAddress().toString().c_str());
        return false;
    }
    
    // Log detection for debugging regression
    // LOGD("Device detected: %s (Type: %d)", device->getAddress().toString().c_str(), (int)detection.type);
    
    telemetry.advMatchedNamePrefix++;
    
    // Format MAC address (avoid std::string allocation)
    char macBuf[18];
    const uint8_t* val = device->getAddress().getVal();
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             val[5], val[4], val[3], val[2], val[1], val[0]);

    // Whitelist check
    bool isWhitelisted = whitelist.isWhitelisted(macBuf);
    if (isWhitelisted) {
        telemetry.advWhitelisted++;
    }
    
    // In targeted mode: skip non-whitelisted devices early
    if (targetedMode && !whitelist.isEmpty() && !isWhitelisted) {
        telemetry.advSkippedTargeted++;
        return false;
    }
    
    bool shouldCallback = discoveryMode || isWhitelisted;
    
    // Parse data based on device type
    bool encrypted = false;
    TpData data = BleDataParser::parse(device, detection, telemetry, &encrypted);
    
    // Log encrypted BTHome warning (once per minute)
    if (encrypted) {
        LOGW_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS,
                       "BTHome encrypted packet from %s - disable encryption in PVVX settings",
                       macBuf);
    }
    
    if (data.valid) {
        telemetry.advParsedValid++;
        int rssi = device->getRSSI();
        const uint32_t nowMs = millis();

        LOGD_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS,
                       "Data valid for %s: Temp=%.1f Hum=%.1f",
                       macBuf,
                       data.temperature,
                       data.humidity);

        // Throttling check (delegated to BleScanner)
        if (scanner.shouldReport(macBuf, data.temperature, data.humidity, data.battery, rssi, nowMs)) {
            if (callback && shouldCallback) {
                telemetry.advCallback++;
                callback(macBuf, data, rssi);
            }
        }
        
        if (!isWhitelisted) {
            bool updated = scanner.updateDiscoveryCache(macBuf, data.temperature, data.humidity, data.battery, rssi, nowMs);
            if (!updated) {
                 telemetry.cacheDrops++;
                 LOGW("Failed to update discovery cache for %s (Full or Rejected)", macBuf);
            }
        }
        return true;
    } else {
        telemetry.parserErrors++;
        // LOGW("Data INVALID for %s (Type: %d)", macBuf, (int)detection.type);
    }
    
    return false;
}

} // namespace BLE
