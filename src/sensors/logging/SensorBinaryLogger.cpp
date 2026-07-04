#include "SensorBinaryLogger.h"

#include "../../config/App.h"
#include "core/config/ConfigManager.h"
#include "../../config/System.h"
#include "../../system/datalogger/BinaryDataLogger.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"
#include "PsramLogBuffer.h"

#include <algorithm>
#include <cmath>


#undef LOG_TAG
#define LOG_TAG "Sensor"

namespace SENSORS {

namespace {
    // Derive the flash smoothing window from time, not from a magic record
    // count. This keeps the diagnostic meaning of one decimated flash sample
    // stable after changing the sensor read cadence.
    constexpr size_t kFlashAggregationWindowSamples = SENSOR::FLASH_AGGREGATION_WINDOW_SAMPLES;
    static_assert(kFlashAggregationWindowSamples > 0, "Flash aggregation window must contain at least one sample");
    static_assert(kFlashAggregationWindowSamples <= SensorBinaryLogger::PSRAM_BUFFER_RECORDS,
                  "Flash aggregation window must fit inside PSRAM history buffer");

    DATALOG::BinaryLogRecord makeBinaryRecord(const SensorSnapshot& snap) {
        DATALOG::BinaryLogRecord record;
        record.timestamp = static_cast<uint32_t>(time(nullptr));
        if (record.timestamp < SensorBinaryLogger::MIN_VALID_EPOCH) {
           // Intentional: AP/offline mode may run without SNTP for extended periods.
           // We still keep the telemetry path alive and preserve sample continuity,
           // including the decimated flash log, instead of suppressing persistence
           // until wall-clock time becomes valid. API/consumer layers must tolerate
           // pre-NTP timestamps and treat them as "time unavailable" rather than
           // a sensor pipeline failure.
        }

        record.co2 = snap.co2;
        record.temp_10x = DATALOG::floatToInt16_10x(snap.temp);
        record.humid_10x = DATALOG::floatToUInt16_10x(snap.humid);
        return record;
    }

    bool computeMedianUint16(uint16_t* values, size_t count, uint16_t& outMedian) {
        if (count == 0 || values == nullptr) return false;
        const size_t mid = count / 2;
        std::nth_element(values, values + mid, values + count);
        const uint16_t upper = values[mid];
        if (count % 2 == 1) {
            outMedian = upper;
            return true;
        }
        const uint16_t lower = *std::max_element(values, values + mid);
        const uint32_t sum = static_cast<uint32_t>(lower) + static_cast<uint32_t>(upper);
        outMedian = static_cast<uint16_t>((sum + 1) / 2);
        return true;
    }

    bool computeMedianFloat(float* values, size_t count, float& outMedian) {
        if (count == 0 || values == nullptr) return false;
        const size_t mid = count / 2;
        std::nth_element(values, values + mid, values + count);
        const float upper = values[mid];
        if (count % 2 == 1) {
            outMedian = upper;
            return true;
        }
        const float lower = *std::max_element(values, values + mid);
        outMedian = (lower + upper) * 0.5f;
        return true;
    }
} // namespace

void SensorBinaryLogger::begin() {
    // Hot-path telemetry now pushes every valid SCD4x sample into PSRAM.
    // Keep this buffer lightweight and let UI/history depth be tuned separately.
    if (!PsramLogBuffer::begin(PSRAM_BUFFER_RECORDS)) {
        LOGE("Failed to init PSRAM buffer");
    }
}

void SensorBinaryLogger::pushSnapshot(const SensorSnapshot& snap) {
    const DATALOG::BinaryLogRecord record = makeBinaryRecord(snap);

    // Hot path: every valid read is retained in PSRAM for live history/charting.
    if (!PsramLogBuffer::push(record)) {
        LOGW("PSRAM Buffer push failed");
    }
}

void SensorBinaryLogger::writeSnapshot(const SensorSnapshot& snap, PhaseStatus& outStatus, bool forceFlash) {
    outStatus.start_ms = millis();
    outStatus.ok = false;
    outStatus.error_code = nullptr;
    outStatus.error_detail = nullptr;

    // [COLD PATH] Write to LittleFS ONLY every 20 minutes (Decimated)
    static uint32_t lastFlashWrite = 0;
    bool flashError = false;
    
    // Check for overflow safe interval check
    bool timeToLog = (millis() - lastFlashWrite >= FLASH_LOG_INTERVAL_MS);
    
    if (forceFlash || timeToLog) {
        if (forceFlash) {
            LOGI("BIN: Forced logging snapshot to Flash");
        } else {
            LOGI("BIN: Logging snapshot to Flash (20 min interval)");
        }

        SensorSnapshot flashSnap = snap;
        if (timeToLog && !forceFlash) {
            // Use the recent PSRAM history as the source of truth for the flash
            // aggregate. The live path stores every valid 5s sample there, while
            // flash remains intentionally sparse to reduce wear and FS traffic.
            DATALOG::BinaryLogRecord recordsBuf[kFlashAggregationWindowSamples];
            const size_t retrievedCount = PsramLogBuffer::copyLastRecords(
                recordsBuf,
                kFlashAggregationWindowSamples
            );
            if (retrievedCount > 0) {
                uint16_t co2Values[kFlashAggregationWindowSamples];
                float tempValues[kFlashAggregationWindowSamples];
                float humidValues[kFlashAggregationWindowSamples];

                size_t co2Count = 0;
                size_t tempCount = 0;
                size_t humidCount = 0;

                for (size_t i = 0; i < retrievedCount; i++) {
                    const auto& rec = recordsBuf[i];
                    const uint16_t co2 = rec.co2;
                    const float temp = DATALOG::int16ToFloat_10x(rec.temp_10x);
                    const float humid = DATALOG::uint16ToFloat_10x(rec.humid_10x);

                    if (co2 >= SENSOR::SCD4X::CO2_MIN_PPM && co2 <= SENSOR::SCD4X::CO2_MAX_PPM &&
                        co2Count < kFlashAggregationWindowSamples) {
                        co2Values[co2Count++] = co2;
                    }
                    if (std::isfinite(temp) && temp >= SENSOR::SCD4X::TEMP_MIN_C && temp <= SENSOR::SCD4X::TEMP_MAX_C &&
                        tempCount < kFlashAggregationWindowSamples) {
                        tempValues[tempCount++] = temp;
                    }
                    if (std::isfinite(humid) && humid >= SENSOR::SCD4X::HUMID_MIN_PCT && humid <= SENSOR::SCD4X::HUMID_MAX_PCT &&
                        humidCount < kFlashAggregationWindowSamples) {
                        humidValues[humidCount++] = humid;
                    }
                }

                uint16_t co2Median = 0;
                float tempMedian = 0.0f;
                float humidMedian = 0.0f;

                if (computeMedianUint16(co2Values, co2Count, co2Median)) {
                    flashSnap.co2 = co2Median;
                }
                if (computeMedianFloat(tempValues, tempCount, tempMedian)) {
                    flashSnap.temp = tempMedian;
                }
                if (computeMedianFloat(humidValues, humidCount, humidMedian)) {
                    flashSnap.humid = humidMedian;
                }

                LOGI("BIN: Flash write using median from %u records (valid: CO2=%u, T=%u, RH=%u)",
                     static_cast<unsigned>(retrievedCount),
                     static_cast<unsigned>(co2Count),
                     static_cast<unsigned>(tempCount),
                     static_cast<unsigned>(humidCount));
                LOGD("BIN: Computed medians -> CO2: %u ppm, T: %.2f C, RH: %.2f %%",
                     flashSnap.co2,
                     flashSnap.temp,
                     flashSnap.humid);
            }
        }

        const uint32_t flashAttemptMs = millis();
        DATALOG::BinaryLogWriteResult writeResult =
            DATALOG::BinaryDataLogger::logSensorData(flashSnap.co2, flashSnap.temp, flashSnap.humid);

        // New behavior: a low-space append does not immediately run full
        // rotation while the sensor path is still inside one long global FS
        // lock. We first get a NeedsMaintenance signal, run the slow cleanup
        // path separately, then retry the append once.
        if (writeResult == DATALOG::BinaryLogWriteResult::NeedsMaintenance) {
            const size_t bytesNeeded = sizeof(DATALOG::BinaryLogRecord) + sizeof(DATALOG::BinaryFileHeader);
            if (DATALOG::BinaryDataLogger::serviceStorageMaintenance(bytesNeeded)) {
                writeResult = DATALOG::BinaryDataLogger::logSensorData(
                    flashSnap.co2, flashSnap.temp, flashSnap.humid
                );
            }
        }

        // Advance the flash cadence after a real FS attempt, even on failure.
        // Busy mutex is the only case where we intentionally keep the cadence
        // unchanged, because no append or maintenance work could even start.
        if (writeResult != DATALOG::BinaryLogWriteResult::Busy) {
            lastFlashWrite = flashAttemptMs;
        }

        if (writeResult == DATALOG::BinaryLogWriteResult::Success) {
            outStatus.error_code = nullptr;
        } else if (writeResult == DATALOG::BinaryLogWriteResult::Busy) {
            LOGW("BIN: Global FS mutex busy/unavailable, flash write skipped (will retry next cycle)");
            outStatus.error_code = "FS_BUSY";
            flashError = true;
        } else if (writeResult == DATALOG::BinaryLogWriteResult::NeedsMaintenance) {
            outStatus.error_code = "FS_LOW_SPACE";
            flashError = true;
        } else {
            outStatus.error_code = "FS_WRITE_FAIL";
            flashError = true;
        }
    }

    outStatus.duration_ms = millis() - outStatus.start_ms;
    outStatus.ok = !flashError;
}

}  // namespace SENSORS
