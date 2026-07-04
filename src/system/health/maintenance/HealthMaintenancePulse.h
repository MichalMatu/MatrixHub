/**
 * @file HealthMaintenancePulse.h
 * @brief Periodic maintenance checks extracted from Application::loop()
 *
 * This scheduler coordinates low-frequency housekeeping work for the health
 * subsystem without turning SystemHealth into a timing-heavy god object.
 */

#pragma once

#include <Arduino.h>

namespace SYSTEM {

/**
 * @brief Periodic maintenance scheduler for health-related housekeeping.
 *
 * Handles timing and execution of:
 * - Heap probe polling
 * - Heap safety checks and hygiene triggers
 */
class HealthMaintenancePulse {
public:
    /**
     * Initialize maintenance timing.
     * Call once during setup.
     */
    static void begin();

    /**
     * Execute periodic maintenance work.
     * Call from main loop - handles its own timing.
     */
    static void update();

    /**
     * Reset all timers (for testing or after deep sleep wake).
     */
    static void reset();

    static constexpr uint32_t kHeapProbePollMs = 5000;
    static constexpr uint32_t kHeapCheckMs = 30000;
    static constexpr uint32_t kCriticalHeapRestartBytes = 15000;

private:
    static uint32_t _lastHeapProbePollMs;
    static uint32_t _lastHeapCheckMs;
    static uint8_t _criticalHeapFaultCount;
    static bool _initialized;
};

}  // namespace SYSTEM
