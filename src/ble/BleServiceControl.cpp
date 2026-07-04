/**
 * @file BleServiceControl.cpp
 * @brief BLE runtime controls and config propagation for scanner-only mode.
 */

#include "BleService.h"

#include "scanner/BleScanner.h"
#include "../system/logging/Logging.h"
#include "../system/rtc/types/RtcBleTypes.h"

#include <WiFi.h>

#undef LOG_TAG
#define LOG_TAG "BLE"

namespace BLE {

bool BleService::updateConfig(const BleConfig& config) {
    const bool runtimeWasRunning = _lifecycle.isRunning();
    _config = config;

    if (!_config.enabled) {
        if (_lifecycle.isRunning()) {
            stop();
        }
        return !_lifecycle.isRunning();
    }

    // If runtime drift left BLE disabled while config says "enabled", start it
    // again instead of silently accepting a persisted/live mismatch.
    if (!runtimeWasRunning) {
        if (!begin()) {
            LOGW("Failed to start BLE scanner runtime after enabling");
            return false;
        }

        // begin() already bootstraps scanner modules, synchronizes the current
        // whitelist from RTC, and applies the initial scanner state. Returning
        // here avoids replaying the same whitelist/startScan path again in the
        // very same enable transaction.
        return _lifecycle.isRunning();
    }

    refreshWhitelist();

    const wifi_mode_t wifiMode = WiFi.getMode();
    const bool apMode = (wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA);
    if (apMode && !kAllowScannerInApMode) {
        setScannerEnabled(false);
        return _lifecycle.isRunning();
    }

    setScannerEnabled(true);
    return _lifecycle.isRunning();
}

void BleService::refreshWhitelist() {
    if (!_pScanner) {
        return;
    }

    // Settings updates still fan out through a second whitelist-refresh
    // handler after the main enabled/disabled runtime transition. Once BLE was
    // turned off, do not let that trailing handler re-touch scanner state:
    // BleScanner::updateWhitelist() can otherwise decide to restart scanning
    // purely because the persisted whitelist is non-empty.
    if (!_config.enabled || !_lifecycle.isRunning()) {
        LOGD("refreshWhitelist skipped: BLE runtime disabled or not running");
        return;
    }

    RTC::BleData cfg = RTC::copyConfigSection(&RTC::ConfigStore::ble);
    _pScanner->syncStateFromRtc();
    _pScanner->updateWhitelist(cfg.sensors, cfg.sensorCount);
}

void BleService::setScannerEnabled(bool enabled) {
    LOGD("setScannerEnabled(%s): _pScanner=%p, lifecycle.running=%d",
         enabled ? "true" : "false", static_cast<void*>(_pScanner), _lifecycle.isRunning());

    if (!_pScanner || !_lifecycle.isRunning()) {
        LOGW("setScannerEnabled: scanner or lifecycle not ready");
        return;
    }

    if (enabled) {
        RTC::BleData cfg = RTC::copyConfigSection(&RTC::ConfigStore::ble);
        refreshWhitelist();
        if (cfg.sensorCount > 0 || _pScanner->isDiscoveryActive()) {
            _pScanner->startScan();
        }
        const bool scanning = _pScanner->isScanning();
        _status.setScannerActive(scanning);
        if (scanning) {
            LOGD("Scanner enabled, scanning started");
        } else {
            LOGI("Scanner enabled, but no active scan is needed right now");
        }
    } else {
        _pScanner->stopScan();
        _status.setScannerActive(_pScanner->isScanning());
        LOGD("Scanner disabled");
    }
}

void BleService::setDiscoveryCallback(BleScanner::DiscoveryCallback cb) {
    _discoveryCallback = cb;
    if (_pScanner) {
        LOGI("Setting discovery callback on scanner instance");
        _pScanner->setDiscoveryCallback(cb);
    } else {
        LOGI("Discovery callback cached until scanner initialization");
    }
}

bool BleService::startDiscovery(uint32_t timeoutMs) {
    if (!_config.enabled || !_pScanner || !_lifecycle.isRunning()) {
        LOGW("startDiscovery ignored: BLE disabled, scanner missing, or lifecycle not ready");
        return false;
    }

    _pScanner->setDiscoveryMode(true, timeoutMs);
    _pScanner->startScan();

    const bool scanning = _pScanner->isScanning();
    _status.setScannerActive(scanning);

    if (!scanning) {
        _pScanner->setDiscoveryMode(false, 0);
        LOGW("Discovery requested but BLE scan did not start");
        return false;
    }

    return true;
}

void BleService::stopDiscovery() {
    if (!_pScanner) {
        return;
    }

    _pScanner->setDiscoveryMode(false, 0);
    if (_config.enabled) {
        setScannerEnabled(true);
        return;
    }

    _pScanner->stopScan();
    _status.setScannerActive(_pScanner->isScanning());
}

bool BleService::isDiscoveryActive() const {
    if (_pScanner) {
        return _pScanner->isDiscoveryActive();
    }
    return false;
}

} // namespace BLE
