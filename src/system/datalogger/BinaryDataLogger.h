#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "BinaryFormat.h"
#include "BinaryLoggerHelpers.h"

namespace DATALOG {

// The logger now distinguishes "FS is currently busy" from "write would need
// storage maintenance". This lets the sensor task stay responsive and avoid
// forcing a long rotate/cleanup under one global lock on the hot path.
enum class BinaryLogWriteResult : uint8_t {
    Success = 0,
    Busy,
    NeedsMaintenance,
    Error,
};

/**
 * Binary data logger - writes sensor snapshots to daily .bin files
 * Uses BinaryLoggerHelpers for file operations
 */
class BinaryDataLogger {
public:
    static void begin();
    // Fast path: attempt only a short, bounded append. If storage is low, the
    // caller gets NeedsMaintenance and can trigger cleanup separately.
    static BinaryLogWriteResult logSensorData(uint16_t co2, float temp, float humid);
    static void logBatch(const BinaryLogRecord* records, size_t count);
    // Slow path: run deferred cleanup/rotation outside the append attempt so
    // API readers and other FS users are not blocked behind one long hold.
    static bool serviceStorageMaintenance(size_t bytesNeeded = 0, const char* excludePath = nullptr);
    static bool checkRotate(size_t bytesNeeded = 0, const char* excludePath = nullptr);
};

}  // namespace DATALOG
