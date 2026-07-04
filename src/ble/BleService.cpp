/**
 * @file BleService.cpp
 * @brief BLE facade status/query methods for scanner-only mode.
 */

#include "BleService.h"

#include "scanner/BleScanner.h"

namespace BLE {

bool BleService::isRunning() const {
    return _status.isRunning();
}

bool BleService::isConnected() const {
    return false;
}

BleStatus BleService::getStatus() const {
    return _status.getStatus();
}

bool BleService::getCachedDeviceData(
    const char* mac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen
) const {
    if (_pScanner) {
        return _pScanner->getCachedDeviceData(mac, outTemp, outHumid, outBatt, outRssi, outLastSeen);
    }
    return false;
}

bool BleService::getCachedDeviceDataAt(
    size_t slotIndex,
    const char*& outMac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen
) const {
    if (_pScanner) {
        return _pScanner->getCachedDeviceDataAt(slotIndex, outMac, outTemp, outHumid, outBatt, outRssi, outLastSeen);
    }
    return false;
}

size_t BleService::getCachedDeviceSlots() const {
    if (_pScanner) {
        return _pScanner->getCachedDeviceSlots();
    }
    return 0;
}

bool BleService::getDiscoveryEntryAt(
    size_t idx,
    const char*& outMac,
    float& outTemp,
    float& outHumid,
    uint8_t& outBatt,
    int8_t& outRssi,
    uint32_t& outLastSeen
) const {
    if (_pScanner) {
        return _pScanner->getDiscoveryEntryAt(idx, outMac, outTemp, outHumid, outBatt, outRssi, outLastSeen);
    }
    return false;
}

size_t BleService::getDiscoverySlotCount() const {
    if (_pScanner) {
        return _pScanner->getDiscoverySlotCount();
    }
    return 0;
}

void BleService::clearDiscoveryCache() {
    if (_pScanner) {
        _pScanner->clearDiscoveryCache();
    }
}

} // namespace BLE
