/**
 * @file BleServiceLifecycle.cpp
 * @brief BLE facade lifecycle and module bootstrap for scanner-only mode.
 */

#include "BleService.h"

#include <cstring>

#include "scanner/BleScanner.h"
#include "../system/logging/Logging.h"

#include <WiFi.h>
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "BLE"

namespace BLE {

static void* allocPsram(size_t size) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        LOGE("Failed to allocate %u bytes in PSRAM", (unsigned)size);
        return nullptr;
    }
    memset(ptr, 0, size);
    return ptr;
}

BleService::BleService() = default;

BleService::~BleService() = default;

bool BleService::ensureCoreModules() {
    if (!_pScanner) {
        void* scanMem = allocPsram(sizeof(BleScanner));
        if (!scanMem) {
            return false;
        }
        _pScanner = new (scanMem) BleScanner();
        LOGI("BleScanner allocated in PSRAM");
    }

    return true;
}

bool BleService::begin() {
    if (_lifecycle.isRunning()) {
        LOGW("BLE service already running");
        return true;
    }

    if (!_config.enabled) {
        LOGI("BLE scanner runtime disabled by config");
        return false;
    }

    if (!ensureCoreModules()) {
        LOGE("BLE startup aborted: required PSRAM modules are missing");
        return false;
    }

    LOGI("Initializing BLE scanner service...");

    if (!initStack()) {
        return false;
    }

    initModules();

    if (!_apStartEventRegistered && !kAllowScannerInApMode) {
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            (void)event;
            (void)info;
            LOGI("WiFi AP started - requesting BLE scanner stop for radio coexistence");
            _pendingApScannerStop.store(true, std::memory_order_release);
        }, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_START);
        _apStartEventRegistered = true;
    }

    startInitialState();
    return _lifecycle.isRunning();
}

bool BleService::initStack() {
    if (!_lifecycle.begin(kDeviceName)) {
        LOGE("BLE lifecycle init failed");
        return false;
    }
    return true;
}

void BleService::initModules() {
    _pScanner->syncStateFromRtc();
    _pScanner->setBleStats(&RTC::runtimeStats.ble);
    _pScanner->begin([](const char* mac, const TpData& data, int rssi) {
        static uint32_t lastThermoLogMs = 0;
        const uint32_t now = millis();
        if (now - lastThermoLogMs >= TASK_MONITOR::BLE_WARNING_THROTTLE_MS) {
            LOGD("ThermoPro data: mac=%s, temp=%d, hum=%d%%, rssi=%d (throttled)",
                 mac, (int)data.temperature, (int)data.humidity, rssi);
            lastThermoLogMs = now;
        }
    });
    if (_discoveryCallback) {
        _pScanner->setDiscoveryCallback(_discoveryCallback);
    }
    LOGI("Scanner initialized: _pScanner=%p", static_cast<void*>(_pScanner));
}

void BleService::startInitialState() {
    const wifi_mode_t wifiMode = WiFi.getMode();
    const bool apMode = (wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA);
    if (apMode && !kAllowScannerInApMode) {
        LOGI("Scanner skipped at startup - WiFi AP mode active (radio coexistence)");
    } else if (apMode) {
        LOGI("Enabling scanner subsystem at startup (AP coexistence allowed)");
    } else {
        LOGI("Enabling scanner subsystem at startup");
    }

    if (!(apMode && !kAllowScannerInApMode)) {
        setScannerEnabled(true);
    }

    _status.setRunning(true);
    LOGI("BLE scanner service started successfully (scanner=%s)",
         _status.isScannerActive() ? "ON" : "OFF");
}

void BleService::loop() {
    if (!_status.isRunning()) {
        return;
    }

    if (_pendingApScannerStop.exchange(false, std::memory_order_acq_rel)) {
        if (_status.isScannerActive()) {
            LOGI("Processing deferred AP scanner stop");
            setScannerEnabled(false);
        }
    }

    if (_pScanner) {
        _status.setScannerActive(_pScanner->isScanning());
        _pScanner->flushRuntimeStateIfDirty();
    }
}

void BleService::stop() {
    if (!_lifecycle.isRunning()) {
        return;
    }

    LOGI("Stopping BLE service...");

    if (_pScanner) {
        _pScanner->flushRuntimeStateIfDirty();
        _pScanner->stopScan();
        _pScanner->flushRuntimeStateIfDirty();
    }
    _status.setScannerActive(false);

    _lifecycle.stop();

    _status.setRunning(false);
    _pendingApScannerStop.store(false, std::memory_order_release);

    LOGI("BLE service stopped");
}

void BleService::prepareForSleep() {
    if (!_lifecycle.isRunning()) {
        LOGI("BLE already stopped, skipping pre-sleep");
        return;
    }

    LOGI("BLE preparing for sleep (soft stop)...");

    if (_pScanner) {
        _pScanner->flushRuntimeStateIfDirty();
        _pScanner->stopScan();
        _pScanner->flushRuntimeStateIfDirty();
    }
    _status.setScannerActive(false);

    _status.setRunning(false);
    _pendingApScannerStop.store(false, std::memory_order_release);

    LOGI("BLE pre-sleep complete (soft stop, no deinit)");
}

} // namespace BLE
