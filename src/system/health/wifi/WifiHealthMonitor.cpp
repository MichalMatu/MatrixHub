#include "WifiHealthMonitor.h"
#include <cstring>

namespace SYSTEM {
namespace HEALTH {

WifiHealthMonitor::WifiHealthMonitor() {
    reset();
}

void WifiHealthMonitor::reset() {
    memset(&_health, 0, sizeof(_health));
}

void WifiHealthMonitor::onConnected(const char* ssid, int8_t rssi, uint32_t nowMs) {
    if (_health.bootTimeMs == 0) {
        _health.bootTimeMs = nowMs;
    }
    _health.lastConnectMs = nowMs;
    _health.isConnected = true;
    _health.currentRssi = rssi;
    
    if (ssid) {
        strncpy(_health.lastSsid, ssid, sizeof(_health.lastSsid) - 1);
        _health.lastSsid[sizeof(_health.lastSsid) - 1] = '\0';
    }
}

void WifiHealthMonitor::onDisconnected(uint8_t reason, uint32_t nowMs) {
    _health.lastDisconnectMs = nowMs;
    _health.lastDisconnectReason = reason;
    _health.isConnected = false;
    _health.reconnectCount++;
}

void WifiHealthMonitor::updateRssi(int8_t rssi) {
    _health.currentRssi = rssi;
}

void WifiHealthMonitor::recordFailedRecovery() {
    _health.failedConnectCount++;
}

uint32_t WifiHealthMonitor::getReconnectsPerHour(uint32_t systemUptimeMs) const {
    uint32_t uptimeHours = systemUptimeMs / 3600000;
    if (uptimeHours == 0) uptimeHours = 1;
    return _health.reconnectCount / uptimeHours;
}

bool WifiHealthMonitor::isHealthy(uint32_t systemUptimeMs) const {
    if (!_health.isConnected) {
        return false;
    }
    
    // Check reconnect rate
    if (getReconnectsPerHour(systemUptimeMs) > kMaxReconnectsPerHour) {
        return false;
    }
    
    // Check signal strength
    if (_health.currentRssi < kMinHealthyRssi) {
        return false;
    }
    
    return true;
}

const WiFiHealth& WifiHealthMonitor::getHealth() const {
    return _health;
}

} // namespace HEALTH
} // namespace SYSTEM
