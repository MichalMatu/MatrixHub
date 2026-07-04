#pragma once

#include "../model/SensorTypes.h"

namespace SENSORS {

class SensorBinaryLogger {
public:
    static constexpr uint32_t MIN_VALID_EPOCH = 1000000000;
    static constexpr uint32_t FLASH_LOG_INTERVAL_MS = 20 * 60 * 1000;
    // Keep roughly the same short-term history window after moving the sensor
    // hot path from 10s cadence to 5s cadence, so UI/history depth does not
    // silently shrink by half after a timing-only refactor.
    static constexpr size_t PSRAM_BUFFER_RECORDS = 192;

    static void begin();
    static void pushSnapshot(const SensorSnapshot& snap);
    static void writeSnapshot(const SensorSnapshot& snap, PhaseStatus& outStatus, bool forceFlash = false);
};

}  // namespace SENSORS
