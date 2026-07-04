#pragma once

#include <Arduino.h>
#include "RtcBleTypes.h"
#include "RtcSystemTypes.h"

namespace RTC {

/**
 * Sensor snapshot stored in RTC (POD version without pointers).
 * Mirrors SensorSnapshot from SensorTypes.h but safe for RTC.
 * Size: 2 + 4 + 4 + 4 + 4 = 18 bytes → 20 aligned
 */
struct RtcSensorSnapshot {
    uint16_t co2 = 0;           // CO2 concentration in ppm
    float temp = 0.0f;          // Temperature in °C
    float humid = 0.0f;         // Relative humidity in %RH
    uint32_t timestamp_ms = 0;  // millis() when captured
    uint32_t seq = 0;           // Sequence number
};

/**
 * Phase status stored in RTC (POD version, error as enum index).
 * Size: 1 + 4 + 4 + 1 = 10 bytes → 12 aligned
 */
struct RtcPhaseStatus {
    bool ok = false;
    uint32_t start_ms = 0;
    uint32_t duration_ms = 0;
    uint8_t errorCode = 0;      // Index into error code table (0 = none)
};

/**
 * Sensor state cache in RTC memory.
 * Survives deep sleep - no need to re-read sensor on wake.
 * Size: 4 + 20 + 20 + 12 + 12 + 1 + 4 + 4 = 77 bytes → 80 aligned
 */
struct RtcSensorState {
    uint32_t magic = 0;
    RtcSensorSnapshot latest;
    RtcSensorSnapshot lastGood;
    RtcPhaseStatus lastRead;
    RtcPhaseStatus lastWrite;
    uint8_t lastErrorCode = 0;
    uint32_t lastErrorTimestamp = 0;
    uint32_t crc = 0;
};

constexpr uint32_t kSensorStateMagic = 0x53454E53;  // "SENS"

/**
 * Heap sample for diagnostics (POD).
 * Size: 4 + 4 + 4 + 1 = 13 bytes → 16 aligned
 */
struct RtcHeapSample {
    uint32_t timestampMs = 0;
    uint32_t freeHeap = 0;
    uint32_t largestBlock = 0;
    uint8_t fragmentation = 0;
    uint8_t _pad[3] = {0};
};

/** Number of heap samples to keep in RTC */
constexpr uint8_t kHeapHistorySize = 12;

/**
 * Heap history for diagnostics in RTC.
 * Size: 12 * 16 + 2 + 2 + 4 + 4 = 204 bytes
 */
struct RtcHeapHistory {
    RtcHeapSample samples[kHeapHistorySize];
    uint8_t head = 0;
    uint8_t count = 0;
    uint16_t _pad = 0;
    uint32_t peakFree = 0;
    uint32_t lowestFree = 0;
};

/**
 * Runtime statistics counters in RTC.
 * Survives deep sleep and soft reboot.
 * Size notes are intentionally approximate; use sizeof(RtcRuntimeStats) for
 * diagnostics because this struct is extended by feature runtime caches.
 */
/**
 * BLE scanning telemetry counters.
 * 
 * THREADING NOTE: These counters are incremented from the NimBLE host task
 * (onResult callback) and read from the main loop (RuntimeStatsService, API).
 * On ESP32-S3, 32-bit aligned reads/writes are atomic at the hardware level,
 * so torn reads cannot occur. The lack of std::atomic means the compiler MAY
 * cache values in registers (stale reads), but this is acceptable for telemetry
 * counters where eventual consistency is sufficient.
 * 
 * We intentionally do NOT use std::atomic<uint32_t> here because this struct
 * resides in RTC memory and must remain POD/trivially-copyable for memcpy
 * and CRC integrity operations.
 */
struct RtcBleStats {
    uint32_t advTotal = 0;
    uint32_t advMatchedNamePrefix = 0;
    uint32_t advHasMfgData = 0;      // TP357 manufacturer data
    uint32_t advBtHomeData = 0;      // BTHome service data
    uint32_t advParsedValid = 0;
    uint32_t advWhitelisted = 0;
    uint32_t advCallback = 0;
    uint32_t advSkippedTargeted = 0;
    uint32_t parserErrors = 0;
    uint32_t cacheDrops = 0;
    uint32_t mutexTimeouts = 0;

    // Helper fields for delta calculation (telemetry logging)
    uint32_t lastLogMs = 0;
    uint32_t lastAdvTotal = 0;
    uint32_t lastAdvMatchedNamePrefix = 0;
    uint32_t lastAdvParsedValid = 0;
    uint32_t lastAdvCallback = 0;
};

struct RtcHeartbeatSlotStats {
    uint32_t lastPingMs = 0;
    uint32_t successCount = 0;
    uint32_t failCount = 0;
};

struct RtcRuntimeStats {
    uint32_t magic = 0;
    
    // Webhook stats (16 bytes)
    uint32_t webhookSent = 0;
    uint32_t webhookFailed = 0;
    uint32_t webhookLastSendMs = 0;
    int16_t webhookLastHttpCode = 0;
    uint16_t _webhookPad = 0;
    
    // Pushover stats (16 bytes)
    uint32_t pushoverSent = 0;
    uint32_t pushoverFailed = 0;
    uint32_t pushoverLastSendMs = 0;
    int16_t pushoverLastHttpCode = 0;
    uint16_t _pushoverPad = 0;
    
    // Shelly stats (12 bytes)
    uint32_t shellyCmdExecuted = 0;
    uint32_t shellyCmdFailed = 0;
    uint32_t shellyPollCycles = 0;
    
    // Telegram stats (16 bytes)
    int64_t telegramLastUpdateId = 0;
    uint32_t telegramMsgsSent = 0;
    uint32_t telegramCmdsHandled = 0;
    uint32_t notifWorkerForcedDeletes = 0;
    
    // Sensor stats (12 bytes)
    uint32_t sensorReads = 0;
    uint32_t sensorErrors = 0;
    uint32_t binaryWrites = 0;
    
    // Alarm stats (8 bytes)
    uint32_t alarmsTriggered = 0;
    uint32_t alarmNotifications = 0;

    // UDP Pusher stats (16 bytes)
    uint32_t udpSent = 0;
    uint32_t udpFailed = 0;
    uint32_t udpLastSendMs = 0;
    uint32_t _udpReserved = 0;

    // Heartbeat stats (4 slots * 12 bytes = 48 bytes)
    RtcHeartbeatSlotStats heartbeatSlots[kMaxHeartbeatSlots];
    
    // BLE stats (64 bytes) and runtime-only whitelist readings (128 bytes)
    RtcBleStats ble;
    BleSensorReading bleReadings[kMaxBleSensors];
    
    // Maintenance sleep tracking (12 bytes)
    bool hygieneSleepActive = false;   // Latched before maintenance sleep, cleared after warm wake
    uint8_t _pad = 0;                  // Alignment
    uint16_t hygieneSleepCount = 0;    // Number of maintenance sleeps (uint16 = 65535 max)
    uint32_t lastHygieneSleepMs = 0;   // millis() of last maintenance sleep (for rate limit)
    float lastThermalShutdownTemp = 0; // Temperature (°C) that triggered the last thermal shutdown (0 = none)
    
    uint32_t crc = 0;
};

constexpr uint32_t kRuntimeStatsMagic = 0x52554E54;  // "RUNT"

} // namespace RTC
