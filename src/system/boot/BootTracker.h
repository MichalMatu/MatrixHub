/**
 * @file BootTracker.h
 * @brief Tracks boot/shutdown state for detecting unexpected restarts
 * 
 * Uses RTC memory to persist state across deep sleep and soft-reset cycles.
 * This tracker is intentionally RTC-only: it does not recover counters from NVS
 * after power loss or a cold boot that clears RTC state.
 * Helps identify:
 * - Clean shutdown vs crash
 * - Unexpected restarts (watchdog, brownout, etc.)
 * - Boot count
 */

#pragma once

#include <Arduino.h>

namespace SYSTEM {

/**
 * Boot state stored only in RTC memory
 */
struct __attribute__((packed)) BootState {
    uint32_t magic;              // Validation marker
    uint32_t bootCount;          // Number of boots since the current RTC state was established
    uint32_t lastShutdownMs;     // millis() at last controlled shutdown
    uint32_t lastUptimeMs;       // How long was the last session
    uint8_t lastShutdownReason;  // Enum: clean sleep, restart command, etc.
    uint8_t lastResetReason;     // ESP reset reason from previous boot
    uint16_t unexpectedRestarts; // Count of boots without clean shutdown
    uint32_t freeHeapAtShutdown; // Heap at last shutdown
};

enum class ShutdownReason : uint8_t {
    UNKNOWN = 0,
    CLEAN_SLEEP = 1,           // Normal deep sleep
    RESTART_COMMAND = 2,       // User-initiated restart
    OTA_UPDATE = 3,            // OTA update restart
    FACTORY_RESET = 4,         // Factory reset
    WATCHDOG_RESTART = 5,      // Watchdog-triggered
    LOW_MEMORY = 6,            // Low memory restart
    HYGIENE_SLEEP = 7,         // Short maintenance sleep (heap / thermal recovery)
};

class BootTracker {
public:
    /**
     * Initialize tracker and analyze boot state.
     * If RTC state is missing or invalid, counters start fresh for this cold-boot epoch.
     * Call early in setup()
     */
    static void begin();
    
    /**
     * Record clean shutdown before sleeping/restarting
     * Call before esp_deep_sleep_start() or ESP.restart()
     * 
     * @param reason Why shutting down
     */
    static void recordShutdown(ShutdownReason reason);
    
    /**
     * Get current boot count
     */
    static uint32_t getBootCount();
    
    /**
     * Get count of unexpected restarts
     */
    static uint16_t getUnexpectedRestarts();
    
    /**
     * Check if last boot was from unexpected source
     */
    static bool wasLastBootUnexpected();
    
    /**
     * Get uptime of previous session (if available)
     */
    static uint32_t getLastSessionUptimeMs();

    /**
     * Get the last recorded controlled shutdown reason.
     */
    static uint8_t getLastShutdownReason();

    /**
     * Get the ESP reset reason retained in the previous session snapshot.
     */
    static uint8_t getLastResetReason();

    /**
     * Get the free heap captured at the last controlled shutdown.
     */
    static uint32_t getFreeHeapAtShutdown();
    
    /**
     * Log boot state for debugging
     */
    static void logState();
    
private:
    static BootState _state;
    static BootState _lastSessionState;
    static bool _initialized;
    static bool _lastBootUnexpected;
    
    static constexpr uint32_t kMagic = 0xB007CAFE;  // Validation marker
    
    static void loadFromRtc();
    static void saveToRtc();
    static void resetState();
};

}  // namespace SYSTEM
