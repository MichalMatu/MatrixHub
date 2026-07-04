/**
 * @file WifiHealthTracker.h
 * @brief Track WiFi connection health and stability
 */

#pragma once

#include <Arduino.h>
#include "WifiHealthMonitor.h"
#include <functional>

namespace SYSTEM {
namespace HEALTH {

class WifiHealthTracker {
public:
    /**
     * Initialize WiFi health tracking
     */
    static void begin();
    
    /**
     * Process queued WiFi events and refresh RSSI periodically.
     */
    static void update();
    
    /**
     * WiFi event callbacks
     */
    static void onConnected(const char* ssid, int8_t rssi);
    static void onDisconnected(uint8_t reason);
    
    /**
     * Check if WiFi is healthy (connected with good signal and low reconnect rate)
     */
    static bool isHealthy(uint32_t systemUptimeMs);
    
    /**
     * Bind the coordinated recovery requester owned by WiFiSettingsService.
     */
    static void setRecoveryRequester(std::function<bool(const char* reason)> requester);
    
    /**
     * Queue coordinated WiFi recovery via the owner service.
     * Returns whether the request was accepted/queued, not whether WiFi is
     * already connected again.
     * The old blocking/dual-name API was collapsed into this single path so
     * callers do not assume an immediate reconnect inside the same call stack.
     */
    static bool requestRecovery(const char* reason = "health");
    
    /**
     * Get current WiFi health stats
     */
    static const WiFiHealth& getHealth();
    
    /**
     * Configuration
     */
    static constexpr uint16_t kMaxReconnectsPerHour = WifiHealthMonitor::kMaxReconnectsPerHour;
    static constexpr uint32_t kRecoveryDelayMs = 30000;    // 30s between recovery attempts
    
private:
    static void registerEventHandlers();
    static void queueConnectedEvent();
    static void queueDisconnectedEvent(uint8_t reason);
    static void processPendingEvents(uint32_t nowMs);
    static void reconcileStateIfDue(uint32_t nowMs);
    static void refreshRssiIfDue(uint32_t nowMs);
    
    static WifiHealthMonitor _monitor;
    static uint32_t _lastRecoveryMs;
    static uint32_t _lastRssiPollMs;
    static uint32_t _lastStatePollMs;
    static volatile bool _connectedEventPending;
    static volatile bool _disconnectedEventPending;
    static volatile uint8_t _pendingDisconnectReason;
    static bool _eventHandlersRegistered;
    static std::function<bool(const char* reason)> _recoveryRequester;
};

}  // namespace HEALTH
}  // namespace SYSTEM
