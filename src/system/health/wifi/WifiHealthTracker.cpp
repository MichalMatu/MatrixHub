/**
 * @file WifiHealthTracker.cpp
 * @brief Implementation of WiFi health tracking
 */

#include "WifiHealthTracker.h"
#include "../../../config/System.h"
#include "../../logging/Logging.h"
#include "../../rtc/RtcConfig.h"
#include <WiFi.h>
#include <cstring>
#include <utility>

#undef LOG_TAG
#define LOG_TAG "WifiHealth"

namespace SYSTEM {
namespace HEALTH {

// Static member initialization
WifiHealthMonitor WifiHealthTracker::_monitor;
uint32_t WifiHealthTracker::_lastRecoveryMs = 0;
uint32_t WifiHealthTracker::_lastRssiPollMs = 0;
uint32_t WifiHealthTracker::_lastStatePollMs = 0;
volatile bool WifiHealthTracker::_connectedEventPending = false;
volatile bool WifiHealthTracker::_disconnectedEventPending = false;
volatile uint8_t WifiHealthTracker::_pendingDisconnectReason = 0;
bool WifiHealthTracker::_eventHandlersRegistered = false;
std::function<bool(const char* reason)> WifiHealthTracker::_recoveryRequester;

namespace {

bool refreshFastReconnectCache() {
    if (!WiFi.isConnected()) {
        return false;
    }

    const uint8_t* bssid = WiFi.BSSID();
    const uint8_t channel = WiFi.channel();
    if (!bssid || channel == 0) {
        return false;
    }

    const uint32_t localIp = static_cast<uint32_t>(WiFi.localIP());
    const uint32_t gateway = static_cast<uint32_t>(WiFi.gatewayIP());
    const uint32_t netmask = static_cast<uint32_t>(WiFi.subnetMask());
    const uint32_t ssidCrc = RTC::calculateSsidCrc(WiFi.SSID().c_str());

    RTC::networkState.update(channel, bssid, localIp, gateway, netmask, ssidCrc);

    return true;
}

bool shouldInvalidateFastReconnectCache(uint8_t reason) {
    switch (static_cast<wifi_err_reason_t>(reason)) {
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_ASSOC_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
            return true;
        default:
            return false;
    }
}

const char* disconnectReasonName(uint8_t reason) {
    const char* name = WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason));
    return name ? name : "unknown";
}

}  // namespace

void WifiHealthTracker::begin() {
    _monitor.reset();
    _connectedEventPending = false;
    _disconnectedEventPending = false;
    _pendingDisconnectReason = 0;
    _lastRssiPollMs = 0;
    _lastStatePollMs = 0;

    registerEventHandlers();

    uint32_t now = millis();
    if (WiFi.isConnected()) {
        String currentSsid = WiFi.SSID();
        _monitor.onConnected(currentSsid.c_str(), WiFi.RSSI(), now);
        refreshFastReconnectCache();
        _lastRssiPollMs = now;
        _lastStatePollMs = now;
    }
    
    LOGD("WiFi health tracking initialized (connected=%d)", _monitor.getHealth().isConnected);
}

void WifiHealthTracker::update() {
    const uint32_t now = millis();
    processPendingEvents(now);
    reconcileStateIfDue(now);
    refreshRssiIfDue(now);
}

void WifiHealthTracker::registerEventHandlers() {
    if (_eventHandlersRegistered) {
        return;
    }

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        WifiHealthTracker::queueConnectedEvent();
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        WifiHealthTracker::queueDisconnectedEvent(info.wifi_sta_disconnected.reason);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    _eventHandlersRegistered = true;
}

void WifiHealthTracker::queueConnectedEvent() {
    _disconnectedEventPending = false;
    _connectedEventPending = true;
}

void WifiHealthTracker::queueDisconnectedEvent(uint8_t reason) {
    _pendingDisconnectReason = reason;
    _connectedEventPending = false;
    _disconnectedEventPending = true;
}

void WifiHealthTracker::processPendingEvents(uint32_t nowMs) {
    if (_disconnectedEventPending) {
        _disconnectedEventPending = false;
        onDisconnected(_pendingDisconnectReason);
        _lastStatePollMs = nowMs;
    }

    if (_connectedEventPending) {
        _connectedEventPending = false;

        String currentSsid = WiFi.SSID();
        const int8_t currentRssi = WiFi.RSSI();
        onConnected(currentSsid.c_str(), currentRssi);

        _lastRssiPollMs = nowMs;
        _lastStatePollMs = nowMs;
    }
}

void WifiHealthTracker::reconcileStateIfDue(uint32_t nowMs) {
    if (nowMs - _lastStatePollMs < SYS_HEALTH::WIFI_HEALTH_STATE_RECONCILE_MS) {
        return;
    }
    _lastStatePollMs = nowMs;

    const bool monitorConnected = _monitor.getHealth().isConnected;
    const bool hardwareConnected = WiFi.isConnected();

    if (monitorConnected == hardwareConnected) {
        return;
    }

    if (hardwareConnected) {
        String currentSsid = WiFi.SSID();
        onConnected(currentSsid.c_str(), WiFi.RSSI());
        _lastRssiPollMs = nowMs;
        return;
    }

    onDisconnected(0);
}

void WifiHealthTracker::refreshRssiIfDue(uint32_t nowMs) {
    if (!_monitor.getHealth().isConnected) {
        return;
    }
    if (nowMs - _lastRssiPollMs < SYS_HEALTH::WIFI_HEALTH_RSSI_REFRESH_MS) {
        return;
    }

    _monitor.updateRssi(WiFi.RSSI());
    _lastRssiPollMs = nowMs;
}

void WifiHealthTracker::onConnected(const char* ssid, int8_t rssi) {
    _monitor.onConnected(ssid, rssi, millis());
    const bool cached = refreshFastReconnectCache();
    
    LOGI("WiFi connected: %s, RSSI=%d, reconnects=%u, fastCache=%s",
         _monitor.getHealth().lastSsid,
         rssi,
         _monitor.getHealth().reconnectCount,
         cached ? "READY" : "SKIPPED");
}

void WifiHealthTracker::onDisconnected(uint8_t reason) {
    _monitor.onDisconnected(reason, millis());
    const bool invalidated = shouldInvalidateFastReconnectCache(reason);
    if (invalidated) {
        RTC::networkState.invalidate();
    }
    
    LOGW("WiFi disconnected: reason=%u (%s), total_reconnects=%u%s",
         reason,
         disconnectReasonName(reason),
         _monitor.getHealth().reconnectCount,
         invalidated ? " [fast-cache cleared]" : "");
}

bool WifiHealthTracker::isHealthy(uint32_t systemUptimeMs) {
    return _monitor.isHealthy(systemUptimeMs);
}

void WifiHealthTracker::setRecoveryRequester(std::function<bool(const char* reason)> requester) {
    _recoveryRequester = std::move(requester);
}

bool WifiHealthTracker::requestRecovery(const char* reason) {
    const uint32_t now = millis();

    if (now - _lastRecoveryMs < kRecoveryDelayMs) {
        return false;
    }

    if (WiFi.isConnected()) {
        return true;
    }

    if (!_recoveryRequester) {
        LOGW("WiFi recovery requested without a bound owner service");
        return false;
    }

    const char* safeReason = (reason && reason[0] != '\0') ? reason : "health";
    const bool accepted = _recoveryRequester(safeReason);
    if (accepted) {
        _lastRecoveryMs = now;
        LOGW("Queued coordinated WiFi recovery: reason=%s", safeReason);
        return true;
    }

    _monitor.recordFailedRecovery();
    LOGW("Coordinated WiFi recovery rejected: reason=%s attempt=%u",
         safeReason,
         _monitor.getHealth().failedConnectCount);
    return false;
}

const WiFiHealth& WifiHealthTracker::getHealth() {
    return _monitor.getHealth();
}

}  // namespace HEALTH
}  // namespace SYSTEM
