/**
 * @file RtcConfig.h
 * @brief Centralized retained-config API used across cold boot and deep sleep
 *
 * The current persistence model has three layers:
 * - PSRAM live working copy: RTC::store, used by the running firmware
 * - RTC retained shadow copy: internal backup snapshot used after deep sleep
 * - LittleFS config.json: cold-boot source of truth
 *
 * On cold boot, defaults are applied and config.json hydrates the PSRAM
 * working copy. On warm boot after deep sleep, the retained shadow snapshot is
 * restored into PSRAM and the early filesystem reload path can be skipped.
 *
 * ESP32-S3 has 8KB RTC SLOW memory.
 */

#pragma once

#include <Arduino.h>
#include <functional>
#include "types/RtcBleTypes.h"
#include "types/RtcShellyTypes.h"
#include "types/RtcAlarmTypes.h"
#include "types/RtcSystemTypes.h"
#include "types/RtcNotificationTypes.h"
#include "types/RtcRuntimeTypes.h"
#include "types/RtcNetworkState.h"
#include "types/RtcAirMouseTypes.h"
#include "types/RtcImuTypes.h"
#include "types/RtcMatrixTypes.h"
#include "types/RtcMacroTypes.h"
#include "types/RtcKeyboardTypes.h"
#include "types/RtcCompensationTypes.h"
#include "types/RtcWifiSensingTypes.h"
#include "types/RtcUsbTerminalTypes.h"

namespace RTC {

// ============================================================================
// Constants
// ============================================================================

/** Magic value to detect valid RTC data after power-on reset */
constexpr uint32_t kMagicValid = 0xC0FFEE42;

/**
 * Schema version - INCREMENT when struct layout changes!
 * This forces RTC reload from FS after firmware update.
 * History:
 *   1 - Initial version
 *   2 - Added BleSensorReading[], HeartbeatData, UdpPusherData (Jan 2026)
 *   3 - Added BleDiscoveryEntry[] for RTC-based discovery cache (Jan 2026)
 *   4 - Added AlarmRuntimeState[] to the combined alarm runtime snapshot (Jan 2026)
 *   6 - Added staticPasskey and useStaticPasskey to RTC::BleData (Jan 2026)
 *   7 - Added CRC32 integrity check to ConfigStore (Jan 2026)
 *   8 - Added AirMouseData for Air Mouse feature (Jan 2026)
 *   9 - Added MatrixData for Matrix LED configuration (Jan 2026)
 *  10 - Added Matrix defaults initialization (Jan 2026)
 *  11 - Added Matrix Screen Rotation settings (Jan 2026)
 *  12 - Added WS2812FX Effect settings (Jan 2026)
 *  13 - Added Matrix Secondary Color (Jan 2026)
 *  14 - Added movementEnabled to AirMouseData (Jan 2026)
 *  15 - Fixed movementEnabled default initialization (Jan 2026)
 *  16 - Refactored Macro settings to RTC (Jan 2026)
 *  19 - Added Custom Icons (768 bytes) to MatrixData (Feb 2026)
 *  20 - Added CompensationData for SCD4x (Feb 2026)
 *  21 - Audit fixes: CompensationData field validation (Feb 2026)
 *  22 - Removed dead 'enabled' field from ShellyData (Feb 2026)
 *  23 - Validated alignment, verified no padding needed (Feb 2026)
 *  24 - Audit cleanup: Shelly struct layout change (Feb 2026)
 *  30 - Added UsbTerminalData for USB Remote Terminal (March 2026)
 *  31 - ShellyDevice runtime layout change (zeroPowerCount) (March 2026)
 *  32 - Removed AirMouse enabled flag (March 2026)
 *  33 - Removed Shelly pollIntervalMs from RTC config (March 2026)
 *  34 - Moved Matrix custom icons out of RTC retained config (March 2026)
 *  35 - Moved Heartbeat config out of RTC retained config (March 2026)
 *  36 - Moved Alarm rules out of RTC retained config (March 2026)
 *  37 - Moved BLE discovery cache out of RTC retained config (March 2026)
 *  38 - Moved Shelly config out of RTC retained config (March 2026)
 *  39 - Moved notification secrets/URLs out of RTC retained config (March 2026)
 *  40 - Added KeyboardData for direct keyboard UI/API feature gating (March 2026)
 *  41 - Removed BLE peripheral/passkey retained settings; BLE is scanner-only (March 2026)
 *  42 - Expanded Matrix effectSpeed to uint32_t for long-duration animations (April 2026)
 *  43 - Added WiFi CSI motion alarm retained settings (June 2026)
 *  44 - Added shared IMU runtime/settings state (June 2026)
 *  45 - Added Matrix native effect engine/provider settings (June 2026)
 *  46 - Added Matrix data visualization background settings (June 2026)
 *  47 - Added configurable deep-sleep wake sources/windows to PowerData (July 2026)
 *  48 - Added semantic alarm runtime identities to retained state (July 2026)
 *  49 - Added boot-scoped alarm transition observability fields (July 2026)
 */
constexpr uint32_t kSchemaVersion = 49;

// Shelly Constants moved to types/RtcShellyTypes.h

// BLE Constants moved to types/RtcBleTypes.h

// Alarm Constants moved to types/RtcAlarmTypes.h

// System Constants moved to types/RtcSystemTypes.h

// ============================================================================
// Configuration Structures (POD types only, no pointers/vtables)
// ============================================================================

// System Types moved to types/RtcSystemTypes.h

// BleData moved to types/RtcBleTypes.h

// ShellyData moved to types/RtcShellyTypes.h

// AlarmData moved to types/RtcAlarmTypes.h

// Heartbeat/UDP Types moved to types/RtcSystemTypes.h

// ============================================================================
// Runtime Data (survives deep sleep, not persisted to LittleFS)
// ============================================================================

// Runtime Types moved to types/RtcRuntimeTypes.h

// ============================================================================
// Main RTC Store
// ============================================================================

/**
 * Complete runtime working config.
 *
 * NOTE:
 * - The live ConfigStore instance is allocated in PSRAM, not RTC SLOW memory.
 * - Deep-sleep retention comes from a separate RTC backup snapshot refreshed
 *   in prepareForSleep(), not from the live pointer itself surviving sleep.
 * - This structure has grown over time; historical size estimates quickly become stale.
 * - Use sizeof(ConfigStore), getRuntimeDataSize(), and getTotalRtcUsage() for current values.
 * - logStatus() prints the tracked retained-memory footprint during boot.
 */
struct __attribute__((packed)) ConfigStore {
    // Header
    uint32_t magic;           // Must be kMagicValid for data to be valid
    uint32_t version;         // Schema version for future migrations
    uint32_t crc32;           // Integrity check for the entire store

    // Explicit header size definition for CRC calculation
    static constexpr size_t kHeaderSize = sizeof(magic) + sizeof(version) + sizeof(crc32);

    // Configuration blocks
    LoggingData logging;
    PowerData power;
    NotificationSummaryData notification; // Retained notification summary only
    WifiSensingData wifiSensing;
    BleData ble;
    ShellySummaryData shelly;
    AlarmRuntimeData alarms;
    UdpPusherData udpPusher;  // UDP data pusher (LAN: InfluxDB, Telegraf, etc.)
    AirMouseData airMouse;    // Air Mouse settings
    ImuData imu;              // Shared IMU settings/calibration state
    MatrixData matrix;        // Matrix LED settings
    MacroData macros;         // Macro settings [Refactored]
    KeyboardData keyboard;    // Direct keyboard feature config
    CompensationData compensation; // SCD4x temperature compensation tuning
    UsbTerminalData usbTerminal; // USB Terminal config

    // Padding for future additions (reserve space)
    uint8_t _reserved[160];
};

// ============================================================================
// RTC / retained state instances
// ============================================================================

/** Main configuration store (PSRAM working copy; see store/RtcConfigStore.cpp) */
extern ConfigStore* store; // Encapsulated - use getConfig() or getMutableConfig()

/** Initialize live working store (allocate PSRAM, restore retained backup if valid) */
void init();

/** Runtime sensor state (survives deep sleep, not persisted to FS) */
extern RtcSensorState sensorState;

/** Heap history for diagnostics */
extern RtcHeapHistory heapHistory;

/** Runtime statistics counters */
extern RtcRuntimeStats runtimeStats;

/** Network state cache for WiFi fast-connect after deep sleep */
extern RtcNetworkState networkState;

// ============================================================================
// Accessors (Encapsulation)
// ============================================================================

/** Fast read for Data Plane (no lock - risk of torn reads, but high performance) */
const ConfigStore& getConfig();

/**
 * Safe read for Control Plane (with lock - guaranteed consistency).
 * Use for: Telegram token, WiFi credentials, calibration params.
 * Returns a COPY of the config - safe to use after the lock is released.
 *
 * Current fallback behavior:
 * - logs and returns an unlocked copy if the lock is not initialized
 * - logs and returns an unlocked copy on lock timeout
 */
ConfigStore getConfigSafeCopy();

/** Get Mutable configuration (Use carefully - prefer updateConfig()!) */
ConfigStore& getMutableConfig();

/** Get lock for thread-safe config access (use with ScopeLock) */
SemaphoreHandle_t getLock();

/**
 * Safe read for Control Plane (with lock) using a closure.
 * Avoids copying the entire ConfigStore onto the stack, protecting against Stack Overflow.
 * The reader callback is skipped if the lock cannot be acquired, so callers must
 * initialize any output variables before calling or provide an explicit fallback.
 *
 * Usage:
 *   RTC::withConfig([&](const RTC::ConfigStore& cfg) {
 *       myVar = cfg.usbTerminal.enabled;
 *   });
 *
 * @param reader Lambda/function receiving const ConfigStore reference
 */
void withConfig(const std::function<void(const ConfigStore&)>& reader);

/** Create the config lock mutex (call in setup before scheduler) */
void createLock();

/**
 * Thread-safe config update with automatic integrity refresh.
 * This is the RECOMMENDED way to modify RTC config.
 *
 * Usage:
 *   RTC::updateConfig([](ConfigStore& cfg) {
 *       cfg.matrix.brightness = 42;
 *   });
 *
 * @param updater Lambda/function receiving mutable ConfigStore reference
 * @return true if lock acquired and update applied
 */
bool updateConfig(const std::function<void(ConfigStore&)>& updater);

/**
 * Same as updateConfig but caller already holds the lock.
 * Use when doing multiple operations within a single lock.
 */
void updateConfigLocked(const std::function<void(ConfigStore&)>& updater);

/**
 * Safe section read without exposing the whole ConfigStore to the caller.
 * The callback is skipped if withConfig() cannot acquire the RTC lock.
 */
template <typename T, typename Reader>
void withConfigSection(const T ConfigStore::* member, Reader&& reader) {
    withConfig([&](const ConfigStore& cfg) {
        reader(cfg.*member);
    });
}

/**
 * Copy a single section from the ConfigStore under the RTC lock.
 * Returns a zero-initialized snapshot if the lock could not be acquired.
 */
template <typename T>
T copyConfigSection(const T ConfigStore::* member) {
    T snapshot{};
    withConfigSection(member, [&](const T& value) {
        snapshot = value;
    });
    return snapshot;
}

/**
 * Update a single section under the central RTC config lock.
 * This keeps CRC recalculation and mutation semantics aligned with updateConfig().
 */
template <typename T, typename Updater>
bool updateConfigSection(T ConfigStore::* member, Updater&& updater) {
    return updateConfig([&](ConfigStore& cfg) {
        updater(cfg.*member);
    });
}

// ============================================================================
// API Functions
// ============================================================================

/**
 * Check if RTC data is valid (was properly initialized)
 * Call this early in boot to decide whether to load from FS.
 */
bool isValid();

/**
 * Mark RTC data as valid (call after loading from FS)
 */
void markValid();

/**
 * Initialize RTC store with defaults (used on first boot or after factory reset)
 */
void initDefaults();

/**
 * Validates integrity of runtime structures (SensorState, Stats) using CRC32.
 * If INVALID, resets them to safe empty state.
 * Call this on WARM BOOT.
 */
void validateRuntimeData();

/**
 * Mark a maintenance sleep as pending before entering deep sleep.
 * Covers hygiene, manual-hygiene, and thermal-critical paths.
 *
 * The helper is idempotent while a maintenance sleep is already pending:
 * it will not double-increment the counter if multiple callers race before sleep.
 *
 * @param nowMs Current millis() used for retained diagnostics
 * @param thermalShutdownTemp Optional temperature for thermal-critical sleeps
 * @return true when a new maintenance sleep was latched, false if one was already pending
 */
bool markMaintenanceSleepPending(uint32_t nowMs, float thermalShutdownTemp = 0.0f);

/**
 * Clear the retained maintenance-sleep flag after a successful warm wake.
 *
 * @return true if the flag was set and has been cleared
 */
bool consumeMaintenanceWakeFlag();

/**
 * Prepare retained state before deep sleep.
 *
 * This refreshes CRCs for runtime RTC structures and writes a fresh shadow
 * snapshot of the PSRAM working config into RTC. If that snapshot cannot be
 * refreshed safely, the retained backup is invalidated so the next wake falls
 * back to the cold-boot filesystem path instead of trusting stale state.
 */
void prepareForSleep();

/**
 * Get logical bytes used by one ConfigStore snapshot
 */
constexpr size_t getStoreSize() { return sizeof(ConfigStore); }

/**
 * Get bytes used by the retained backup slot (metadata + ConfigStore payload).
 */
constexpr size_t getBackupSlotSize() {
    return sizeof(ConfigStore) + (sizeof(uint32_t) * 5);
}

/**
 * Get total bytes used by all RTC runtime data
 * Includes only runtime structs, not retained config backups.
 */
constexpr size_t getRuntimeDataSize() {
    return sizeof(RtcSensorState) + sizeof(RtcHeapHistory) +
           sizeof(RtcRuntimeStats) + sizeof(RtcNetworkState);
}

/**
 * Get tracked LP SRAM usage for RTC config backups + runtime data.
 * Note: This is still a compile-time estimate. Actual usage may include
 * additional RTC variables from other modules.
 */
constexpr size_t getTotalRtcUsage() {
    return getBackupSlotSize() + getRuntimeDataSize();
}

/**
 * Total RTC SLOW memory available on ESP32-S3
 * (8KB Slow + 8KB Fast, but Fast is reset on deep sleep in some configs,
 * Slow is guaranteed for persistence).
 */
constexpr size_t kLpSramTotal = 8192;  // 8KB on S3

static_assert(getTotalRtcUsage() <= kLpSramTotal,
              "Tracked RTC retained state exceeds ESP32-S3 LP SRAM");

/**
 * Log current RTC usage and config status
 */
void logStatus();

} // namespace RTC
