#include "BleScanner.h"
#include "BleWhitelistReconciler.h"

#include "../../config/System.h"
#include "../../system/rtc/RtcConfig.h"

#include <NimBLEDevice.h>

#undef LOG_TAG
#define LOG_TAG "BLEScanner"

namespace BLE {

namespace {
bool hasReachedDeadline(uint32_t nowMs, uint32_t deadlineMs) {
    return deadlineMs != 0 && static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}
}

BleScanner::BleScanner() {
    _mutex = xSemaphoreCreateMutex();
    _callbackMutex = xSemaphoreCreateMutex();
    syncStateFromRtc();
}

BleScanner::~BleScanner() {
    if (_callbackMutex) {
        vSemaphoreDelete(_callbackMutex);
        _callbackMutex = nullptr;
    }

    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

void BleScanner::setDiscoveryCallback(DiscoveryCallback cb) {
    if (!_callbackMutex) {
        _discoveryCallback = cb;
        return;
    }

    if (xSemaphoreTake(_callbackMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        LOGW("Callback mutex timeout in setDiscoveryCallback");
        if (_bleStats) {
            _bleStats->mutexTimeouts++;
        }
        return;
    }
    _discoveryCallback = cb;
    xSemaphoreGive(_callbackMutex);
}

void BleScanner::begin(TpDataCallback callback) {
    _callback = callback;
    syncStateFromRtc();
    _pScan = NimBLEDevice::getScan();

    if (!_pScan) {
        LOGE("Failed to get NimBLE scan instance");
        return;
    }

    _pScan->setScanCallbacks(this);
    _pScan->setActiveScan(false); // Passive scanning for WiFi coexistence
    _pScan->setInterval(SENSOR::BLE::SCAN_INTERVAL_MS);
    _pScan->setWindow(SENSOR::BLE::SCAN_WINDOW_MS);
    _pScan->setMaxResults(0); // Unlimited, consumed in callback path
}

void BleScanner::syncStateFromRtc() {
    RTC::BleData snapshot = RTC::copyConfigSection(&RTC::ConfigStore::ble);

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            LOGW("Mutex timeout in syncStateFromRtc");
            if (_bleStats) {
                _bleStats->mutexTimeouts++;
            }
            return;
        }
    }

    _state = snapshot;
    memcpy(_state.readings, RTC::runtimeStats.bleReadings, sizeof(_state.readings));
    _runtimeStateVersion.store(0, std::memory_order_relaxed);
    _flushedRuntimeStateVersion.store(0, std::memory_order_relaxed);
    sanitizeLocked();

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
}

void BleScanner::flushRuntimeStateIfDirty() {
    const uint32_t currentVersion = _runtimeStateVersion.load(std::memory_order_acquire);
    const uint32_t flushedVersion = _flushedRuntimeStateVersion.load(std::memory_order_acquire);
    if (currentVersion == flushedVersion) {
        return;
    }

    RTC::BleSensorReading readingsSnapshot[RTC::kMaxBleSensors];
    uint32_t capturedVersion = flushedVersion;

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            if (_bleStats) {
                _bleStats->mutexTimeouts++;
            }
            return;
        }
    }

    memcpy(readingsSnapshot, _state.readings, sizeof(readingsSnapshot));
    capturedVersion = _runtimeStateVersion.load(std::memory_order_relaxed);

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }

    memcpy(RTC::runtimeStats.bleReadings, readingsSnapshot, sizeof(readingsSnapshot));

    _flushedRuntimeStateVersion.store(capturedVersion, std::memory_order_release);
}

void BleScanner::startScan() {
    LOGV_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS,
                   "startScan() called: _pScan=%p, _scanning=%d",
                   _pScan,
                   _scanning.load(std::memory_order_acquire));
    if (!_pScan) {
        LOGW_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS,
                       "startScan ignored: scan instance not ready");
        return;
    }

    if (_scanning.load(std::memory_order_acquire)) {
        return;
    }

    _scanning.store(true, std::memory_order_release);
    if (_pScan->isScanning()) {
        LOGV_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS,
                       "Scan already running (NimBLE isScanning=true)");
        return;
    }

    LOGD_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS,
                   "Starting BLE scan (%us cycle)",
                   SENSOR::BLE::SCAN_DURATION_SEC);
    if (!_pScan->start(SENSOR::BLE::SCAN_DURATION_SEC, false)) {
        _scanning.store(false, std::memory_order_release);
        LOGW_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS, "Failed to start BLE scan");
    }
}

void BleScanner::onScanComplete(const NimBLEScanResults& results) {
    (void)results;
    if (!_pScan || !_scanning.load(std::memory_order_acquire)) {
        return;
    }

    if (_discoveryMode.load(std::memory_order_acquire)) {
        const uint32_t endTime = _discoveryEndTime.load(std::memory_order_acquire);
        if (hasReachedDeadline(millis(), endTime)) {
            LOGI("Discovery mode timeout, stopping discovery");
            _discoveryMode.store(false, std::memory_order_release);
            _discoveryEndTime.store(0, std::memory_order_release);

            const bool whitelistActive = !_whitelist.isEmpty();
            _targetedMode.store(whitelistActive, std::memory_order_release);
            if (!whitelistActive) {
                LOGI("Whitelist empty after discovery, stopping scan");
                stopScan();
                return;
            }
        }
    }

    if (!_scanning.load(std::memory_order_acquire)) {
        return;
    }

    _pScan->clearResults(); // Prevent heap growth from scan results.
    if (!_pScan->start(SENSOR::BLE::SCAN_DURATION_SEC, false)) {
        _scanning.store(false, std::memory_order_release);
        LOGW_THROTTLED(TASK_MONITOR::BLE_WARNING_THROTTLE_MS, "Failed to restart BLE scan");
    }
}

void BleScanner::setDiscoveryMode(bool enable, uint32_t timeoutMs) {
    LOGI("setDiscoveryMode: %d (timeout=%u ms)", enable, timeoutMs);
    _discoveryMode.store(enable, std::memory_order_release);

    if (enable) {
        _discoveryEndTime.store(millis() + timeoutMs, std::memory_order_release);
        _targetedMode.store(false, std::memory_order_release);
        return;
    }

    _discoveryEndTime.store(0, std::memory_order_release);
    _targetedMode.store(!_whitelist.isEmpty(), std::memory_order_release);
}

void BleScanner::updateWhitelist(const BleSensorConfig* sensors, size_t count) {
    LOGI("updateWhitelist: size=%u, _scanning=%d", (unsigned)count, _scanning.load(std::memory_order_acquire));
    if (!sensors && count > 0) {
        LOGW("updateWhitelist called with null sensors and count=%u, clearing whitelist", (unsigned)count);
        count = 0;
    }

    if (_mutex) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            LOGW("Mutex timeout in updateWhitelist");
            if (_bleStats) {
                _bleStats->mutexTimeouts++;
            }
            return;
        }
    }

    detail::reconcileWhitelistState(_state, _discovered, _discoveryCount, sensors, count);
    _whitelist.update(_state.sensors, _state.sensorCount);
    _runtimeStateVersion.fetch_add(1, std::memory_order_relaxed);

    if (_mutex) {
        xSemaphoreGive(_mutex);
    }

    const bool whitelistActive = !_whitelist.isEmpty();
    const bool discoveryActive = _discoveryMode.load(std::memory_order_acquire);
    _targetedMode.store(whitelistActive && !discoveryActive, std::memory_order_release);

    // Whitelist active -> passive background scan.
    // Empty whitelist -> stop scan unless discovery mode is active.
    if (whitelistActive) {
        if (!_scanning.load(std::memory_order_acquire)) {
            startScan();
        }
        return;
    }

    if (_scanning.load(std::memory_order_acquire) && !discoveryActive) {
        LOGI("Whitelist empty, stopping scan");
        stopScan();
    }
}

void BleScanner::onScanEnd(const NimBLEScanResults& results, int reason) {
    (void)reason;
    onScanComplete(results);
}

void BleScanner::stopScan() {
    if (!_pScan) {
        return;
    }

    // Flip state first so onScanComplete cannot restart while stop is in-flight.
    if (!_scanning.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    _pScan->stop();
    _pScan->clearResults();
    LOGI("BLE scan stopped");
}

void BleScanner::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (!advertisedDevice) {
        return;
    }
    processDevice(advertisedDevice);
}

} // namespace BLE
