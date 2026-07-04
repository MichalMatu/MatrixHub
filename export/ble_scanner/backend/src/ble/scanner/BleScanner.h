#pragma once

#include <NimBLEDevice.h>
#include <functional>
#include <atomic>
#include "../../config/Network.h"
#include "../../system/logging/Logging.h"
#include "../../system/rtc/types/RtcBleTypes.h"
#include "../BleTypes.h"
#include "freertos/FreeRTOS.h"
#include "filters/BleDeviceWhitelist.h"

namespace RTC {
    struct RtcBleStats;
}

namespace BLE {

/**
 * BLE Scanner for ThermoPro devices (Facade)
 * 
 * Delegates to:
 * - BleDeviceWhitelist: MAC whitelist management
 * - BleDeviceCache: Throttling/debouncing
 * 
 * Responsibilities:
 * - NimBLE scan lifecycle (start/stop)
 * - NimBLE callbacks (onResult, onScanComplete)
 * - Discovery mode management
 * - ThermoPro packet parsing (via TpParser)
 */
class BleScanner : public NimBLEScanCallbacks {
public:
    using DiscoveryCallback = std::function<void(const char* mac, float temp, float humid, uint8_t batt, int8_t rssi)>;

    BleScanner();
    virtual ~BleScanner();
    
    /**
     * @brief Reload BLE cache/config snapshot from RTC memory.
     */
    void syncStateFromRtc();
    void setBleStats(RTC::RtcBleStats* stats) { _bleStats = stats; }
    void setDiscoveryCallback(DiscoveryCallback cb);
    
    void begin(TpDataCallback callback);
    void startScan();
    void stopScan();
    void flushRuntimeStateIfDirty();
    bool isScanning() const { return _scanning.load(std::memory_order_acquire); }
    
    // Discovery / Whitelist control
    void setDiscoveryMode(bool enable, uint32_t timeoutMs = 30000);
    void updateWhitelist(const BleSensorConfig* sensors, size_t count);

    // Cache / Throttling logic (Pass-through now)
    bool shouldReport(const char* mac, float temperature, float humidity, uint8_t battery, int8_t rssi, uint32_t nowMs);
    bool updateDiscoveryCache(const char* mac, float temp, float humid, uint8_t batt, int8_t rssi, uint32_t nowMs);
    
    // Cache access for API (whitelisted devices)
    bool getCachedDeviceData(const char* mac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const;

    // Enumerate cached devices by slot index for API/UI scan results (whitelisted)
    bool getCachedDeviceDataAt(size_t slotIndex, const char*& outMac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const;

    bool isDiscoveryActive() const { return _discoveryMode; }


    size_t getCachedDeviceSlots() const { return RTC::kMaxBleSensors; }
    
    // Discovery cache access for API (non-whitelisted discovered devices)
    bool getDiscoveryEntryAt(size_t idx, const char*& outMac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const;
    
    size_t getDiscoverySlotCount() const { return RTC::kMaxBleDiscovered; }
    
    void clearDiscoveryCache();

    // NimBLEScanCallbacks implementation
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
    // Native test stubs still expose onScanComplete(); the real NimBLE 2.3.x API uses onScanEnd().
#if defined(NATIVE_BUILD)
    void onScanComplete(const NimBLEScanResults& results) override;
#else
    void onScanComplete(const NimBLEScanResults& results);
#endif
    void onScanEnd(const NimBLEScanResults& results, int reason) override; // Newer NimBLE

private:
    NimBLEScan* _pScan = nullptr;
    TpDataCallback _callback = nullptr;
    DiscoveryCallback _discoveryCallback = nullptr;
    
    // State (atomic: accessed from both main loop and NimBLE host callback contexts)
    std::atomic<bool> _scanning{false};
    std::atomic<bool> _discoveryMode{false};
    std::atomic<uint32_t> _discoveryEndTime{0};
    std::atomic<bool> _targetedMode{false}; // True when whitelist is active (skip non-whitelisted)
    
    // Delegated components
    BleDeviceWhitelist _whitelist;
    
    // Cache State (RTC Integration)
    RTC::BleData _state{};
    RTC::BleDiscoveryEntry _discovered[RTC::kMaxBleDiscovered]{};
    uint8_t _discoveryCount = 0;
    RTC::RtcBleStats* _bleStats = nullptr; // Injected telemetry (DI)
    mutable SemaphoreHandle_t _mutex = nullptr;
    mutable SemaphoreHandle_t _callbackMutex = nullptr;
    std::atomic<uint32_t> _runtimeStateVersion{0};
    std::atomic<uint32_t> _flushedRuntimeStateVersion{0};
    std::atomic<uint32_t> _lastRuntimeStateFlushMs{0};

    // Cache Helpers
    int findOrAllocateSensorIndex(const char* mac);
    int findOrAllocateDiscoverySlot(const char* mac, uint32_t nowMs);
    void sanitizeLocked();

    // Parses a single device (logic extracted from onResult for readability)
    void processDevice(const NimBLEAdvertisedDevice* device);
};

} // namespace BLE
