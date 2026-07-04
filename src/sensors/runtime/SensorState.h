#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

#include "../model/SensorTypes.h"
#include "../../config/System.h" // For SENSOR::SNAPSHOT_TIMEOUT_MS

namespace SENSORS {

class SensorState {
public:
    static void createInitMutex();  // Call before scheduler starts
    static bool ensureInitialized();

    static SensorSnapshot getSnapshot();
    static SensorSnapshot getLastGoodSnapshot();
    static PhaseStatus getLastReadStatus();
    static PhaseStatus getLastWriteStatus();
    static ErrorInfo getLastErrorInfo();

    static void updateAfterRead(const SensorSnapshot& snap, const PhaseStatus& status);
    static void updateAfterReadFailure(const PhaseStatus& status);
    static void updateAfterWrite(const PhaseStatus& status);

    static SemaphoreHandle_t getSnapshotMutex();

private:
    static SemaphoreHandle_t _initMutex;     // For thread-safe initialization
    static SemaphoreHandle_t _snapshotMutex;

    static SensorSnapshot _latestSnapshot;
    static SensorSnapshot _lastGoodSnapshot;
    static PhaseStatus _lastReadStatus;
    static PhaseStatus _lastWriteStatus;
    static ErrorInfo _lastErrorInfo;

    static std::atomic<bool> _initialized;  // Atomic for dual-core safety
};

}  // namespace SENSORS
