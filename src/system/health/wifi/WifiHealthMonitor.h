#pragma once

#include <cstdint>

namespace SYSTEM {
namespace HEALTH {

struct WiFiHealth {
    uint32_t bootTimeMs;           // When WiFi first connected
    uint32_t lastConnectMs;        // Last successful connection
    uint32_t lastDisconnectMs;     // Last disconnect time
    uint16_t reconnectCount;       // Total reconnects since boot
    uint16_t failedConnectCount;   // Failed connection attempts
    uint8_t lastDisconnectReason;  // WiFi.status() or disconnect reason
    bool isConnected;
    int8_t currentRssi;
    char lastSsid[33];
};

class WifiHealthMonitor {
public:
    WifiHealthMonitor();

    // Event handlers
    void reset();
    void onConnected(const char* ssid, int8_t rssi, uint32_t nowMs);
    void onDisconnected(uint8_t reason, uint32_t nowMs);
    void updateRssi(int8_t rssi);
    void recordFailedRecovery();

    // Query state
    bool isHealthy(uint32_t systemUptimeMs) const;
    const WiFiHealth& getHealth() const;
    uint32_t getReconnectsPerHour(uint32_t systemUptimeMs) const;

    // Configuration
    static constexpr uint16_t kMaxReconnectsPerHour = 10;
    static constexpr int8_t kMinHealthyRssi = -85;

private:
    WiFiHealth _health;
};

} // namespace HEALTH
} // namespace SYSTEM
