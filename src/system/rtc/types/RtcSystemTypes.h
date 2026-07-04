#pragma once

#include <Arduino.h>
#include <esp_log.h>
#include "../RtcDefaultValues.h"

namespace RTC {

/** Maximum number of heartbeat ping slots (supports multiple services like Healthchecks.io, Uptime Kuma, Cronitor) */
constexpr uint8_t kMaxHeartbeatSlots = 4;

/**
 * Logging configuration
 */
struct __attribute__((packed)) LoggingData {
    esp_log_level_t level = Defaults::Logging::Level;
    uint16_t _reserved = 0;  // Preserved for RTC struct layout/CRC compatibility
};

/**
 * Power management configuration
 */
struct __attribute__((packed)) PowerData {
    uint32_t inactivityTimeoutMs = Defaults::Power::InactivityTimeoutMs;
    uint32_t graceAfterBootMs = Defaults::Power::GraceAfterBootMs;
    uint32_t wakeIntervalMs = Defaults::Power::WakeIntervalMs;
    uint32_t timerWakeAwakeMs = Defaults::Power::TimerWakeAwakeMs;
    uint32_t buttonWakeAwakeMs = Defaults::Power::ButtonWakeAwakeMs;
    uint32_t wakeTouchThreshold = Defaults::Power::TouchWakeThreshold;
    uint8_t wakeTouchGpio = Defaults::Power::TouchWakeGpio;
    bool sleepEnabled = Defaults::Power::SleepEnabled;
    bool wakeTimerEnabled = Defaults::Power::WakeTimerEnabled;
    bool wakeButtonEnabled = Defaults::Power::WakeButtonEnabled;
    bool wakeTouchEnabled = Defaults::Power::WakeTouchEnabled;
};
// NotificationData moved to types/RtcNotificationTypes.h

/**
 * Single heartbeat ping slot.
 * Supports any HTTP(S) GET "dead man's switch" service:
 * - Healthchecks.io, Uptime Kuma Push, Cronitor, Better Uptime, etc.
 * Size: 1 + 24 + 192 = 217 bytes per slot
 */
struct __attribute__((packed)) HeartbeatSlot {
    bool enabled = false;
    char name[24] = {0};   // User-defined label (e.g., "Uptime Kuma")
    char url[191] = {0};   // Ping URL (HTTP or HTTPS)
    bool allowInsecure = false; // Only for HTTPS: skip certificate verification

    bool isValid() const {
        return enabled && url[0] != '\0';
    }
};

/**
 * Heartbeat configuration with multiple slots.
 * All slots share a single global 5-minute ping interval.
 * Size: 4 × 217 + 4 = 872 bytes
 */
struct __attribute__((packed)) HeartbeatData {
    HeartbeatSlot slots[kMaxHeartbeatSlots];
    uint32_t intervalMs = Defaults::Heartbeat::IntervalMs;
};

/**
 * UDP data format for pushing sensor readings
 */
enum class UdpFormat : uint8_t {
    LineProtocol = 0,  // InfluxDB line protocol: sensors,device=matrixhub co2=678i,temp=24.5
    Json = 1,          // JSON: {"co2":678,"temp":24.5,"humidity":45.2}
    Csv = 2            // CSV: 678,24.5,45.2
};

/**
 * UDP pusher configuration.
 * Sends sensor data to local LAN server (InfluxDB, Telegraf, custom).
 * Size: 1 + 64 + 2 + 1 + 4 = 72 bytes
 */
struct __attribute__((packed)) UdpPusherData {
    bool enabled = Defaults::UdpPusher::Enabled;
    char host[64] = {0};        // Target IP or hostname
    uint16_t port = Defaults::UdpPusher::Port;
    UdpFormat format = static_cast<UdpFormat>(Defaults::UdpPusher::Format);
    uint32_t intervalMs = Defaults::UdpPusher::IntervalMs;
    
    bool isValid() const {
        return enabled && host[0] != '\0' && port > 0;
    }
};

} // namespace RTC
