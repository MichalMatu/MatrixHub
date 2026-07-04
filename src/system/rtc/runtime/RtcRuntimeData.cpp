/**
 * @file RtcRuntimeData.cpp
 * @brief Runtime retained-state validation and sleep/wake bookkeeping
 */

#include "../RtcConfigInternal.h"

#include "../../logging/Logging.h"
#include "../../utils/ScopeLock.h"

#include <esp_rom_crc.h>

#include <cstddef>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "RtcConfig"

namespace RTC {
namespace {

uint32_t calculateSensorStateCrc(const RtcSensorState& state) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&state);
    const size_t len = offsetof(RtcSensorState, crc);
    return esp_rom_crc32_le(0, data, len);
}

uint32_t calculateRuntimeStatsCrc(const RtcRuntimeStats& stats) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&stats);
    const size_t len = offsetof(RtcRuntimeStats, crc);
    return esp_rom_crc32_le(0, data, len);
}

void invalidateWarmBootBackup(const char* reason) {
    // When the final pre-sleep snapshot cannot be refreshed, keeping the older
    // retained backup is riskier than throwing it away. An invalidated backup
    // forces the next boot down the cold/FS reload path instead of reviving a
    // stale warm-boot snapshot that may no longer match the latest saved state.
    detail::invalidateConfigBackupSnapshot();
    LOGW("prepareForSleep: %s; invalidated retained backup so next wake reloads from FS",
         reason ? reason : "unknown");
}

}  // namespace

void validateRuntimeData() {
    const uint32_t sensorCrc = calculateSensorStateCrc(sensorState);
    if (sensorState.magic != kSensorStateMagic || sensorState.crc != sensorCrc) {
        LOGW("RTC SensorState corrupted/empty (Magic=%08X, CRC Read=%08X vs Calc=%08X). Resetting.",
             sensorState.magic,
             sensorState.crc,
             sensorCrc);
        memset(&sensorState, 0, sizeof(sensorState));
        sensorState.magic = kSensorStateMagic;
    } else {
        LOGI("RTC SensorState valid (CRC: %08X)", sensorState.crc);
    }

    const uint32_t statsCrc = calculateRuntimeStatsCrc(runtimeStats);
    if (runtimeStats.magic != kRuntimeStatsMagic || runtimeStats.crc != statsCrc) {
        LOGW("RTC RuntimeStats corrupted/empty (Magic=%08X, CRC Read=%08X vs Calc=%08X). Resetting.",
             runtimeStats.magic,
             runtimeStats.crc,
             statsCrc);
        memset(&runtimeStats, 0, sizeof(runtimeStats));
        runtimeStats.magic = kRuntimeStatsMagic;
    } else {
        LOGI("RTC RuntimeStats valid (CRC: %08X)", statsCrc);
    }
}

bool markMaintenanceSleepPending(uint32_t nowMs, float thermalShutdownTemp) {
    const bool alreadyPending = runtimeStats.hygieneSleepActive;

    runtimeStats.hygieneSleepActive = true;
    runtimeStats.lastHygieneSleepMs = nowMs;

    if (!alreadyPending && runtimeStats.hygieneSleepCount < UINT16_MAX) {
        runtimeStats.hygieneSleepCount++;
    }

    if (thermalShutdownTemp > 0.0f) {
        runtimeStats.lastThermalShutdownTemp = thermalShutdownTemp;
    }

    return !alreadyPending;
}

bool consumeMaintenanceWakeFlag() {
    if (!runtimeStats.hygieneSleepActive) {
        return false;
    }

    runtimeStats.hygieneSleepActive = false;
    LOGI("Maintenance wake detected - cleared retained hygiene flag");
    return true;
}

void prepareForSleep() {
    sensorState.magic = kSensorStateMagic;
    sensorState.crc = calculateSensorStateCrc(sensorState);

    runtimeStats.magic = kRuntimeStatsMagic;
    runtimeStats.crc = calculateRuntimeStatsCrc(runtimeStats);

    if (!store) {
        invalidateWarmBootBackup("config store unavailable");
        return;
    }

    SemaphoreHandle_t lockHandle = getLock();
    if (!lockHandle) {
        invalidateWarmBootBackup("config lock not initialized");
        return;
    }

    // Sleep entry is the one place where a slightly longer wait is worth it:
    // once we pass this point, PSRAM-backed ConfigStore is gone and the next
    // warm wake will trust only the retained backup snapshot.
    SYSTEM::ScopeLock lock(lockHandle, detail::sleepSnapshotLockTimeoutTicks());
    if (!lock.isLocked()) {
        invalidateWarmBootBackup("config lock timeout");
        return;
    }

    detail::refreshConfigIntegrity(*store);
    (void)detail::saveConfigBackupSnapshot(*store);
}

}  // namespace RTC
