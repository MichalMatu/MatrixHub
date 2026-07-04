/**
 * @file BleService.h
 * @brief Scanner-only BLE service facade.
 */

#pragma once

#include <Arduino.h>
#include <atomic>

#include "../config/Network.h"
#include "BleTypes.h"
#include "core/BleLifecycleManager.h"
#include "core/BleStatusManager.h"
#include "scanner/BleScanner.h"

namespace BLE {

class BleService {
public:
    BleService();
    ~BleService();

    bool begin();
    void loop();
    void stop();
    void prepareForSleep();
    bool isRunning() const;
    bool isConnected() const;

    void setScannerEnabled(bool enabled);
    void setDiscoveryCallback(BleScanner::DiscoveryCallback cb);

    BleStatus getStatus() const;

    bool getCachedDeviceData(const char* mac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const;
    bool getCachedDeviceDataAt(size_t slotIndex, const char*& outMac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const;
    size_t getCachedDeviceSlots() const;

    bool getDiscoveryEntryAt(size_t idx, const char*& outMac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const;
    size_t getDiscoverySlotCount() const;
    void clearDiscoveryCache();

    bool startDiscovery(uint32_t timeoutMs = 30000);
    void stopDiscovery();
    bool isDiscoveryActive() const;
    void refreshWhitelist();

    bool updateConfig(const BleConfig& config);

private:
    bool ensureCoreModules();
    bool initStack();
    void initModules();
    void startInitialState();

    BleLifecycleManager _lifecycle;
    BLE::BleStatusManager _status;
    BleScanner* _pScanner = nullptr;
    BleScanner::DiscoveryCallback _discoveryCallback = nullptr;
    std::atomic<bool> _pendingApScannerStop{false};
    bool _apStartEventRegistered = false;
    BleConfig _config;
};

}  // namespace BLE
