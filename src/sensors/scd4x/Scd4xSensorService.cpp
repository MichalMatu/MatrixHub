#include "Scd4xSensorService.h"

#include "../../config/Hardware.h"
#include "../../utils/hardware/I2cUtils.h"
#include "../../config/System.h" // For SENSOR::SCD4X constants and I2C_SCD4X
#include "../../system/logging/Logging.h"

#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../compensation/CompensationService.h"

#undef LOG_TAG
#define LOG_TAG "SCD4x"

namespace SENSORS {

namespace {
    constexpr uint8_t kI2cProbeAttempts = 2;
    constexpr uint8_t kStopPeriodicAttempts = 3;
    constexpr uint32_t kI2cRetryDelayMs = 100;

    // Error code helper
    const char* errorCodeToString(int16_t error) {
        switch (error) {
            case 0: return "OK";
            case -1: return "I2C_NACK";
            case -2: return "I2C_TIMEOUT";
            case -3: return "CRC_ERROR";
            case 0x010C: return "I2C_ADDRESS_NACK";
            case 0x010D: return "I2C_DATA_NACK";
            case 0x010E: return "I2C_OTHER";
            default: return "UNKNOWN";
        }
    }

    void resetScd4xWireBus() {
        Wire1.end();
        UTILS::HARDWARE::I2cUtils::recoverBus(I2C_SCD4X::SDA_PIN, I2C_SCD4X::SCL_PIN);
        Wire1.begin(I2C_SCD4X::SDA_PIN, I2C_SCD4X::SCL_PIN);
        Wire1.setTimeOut(100); // Prevent infinite wait on NACK/stretch
    }

    bool probeScd4xAddress(bool logInit) {
        for (uint8_t attempt = 1; attempt <= kI2cProbeAttempts; ++attempt) {
            Wire1.beginTransmission(I2C_SCD4X::I2C_ADDRESS);
            const uint8_t wireError = Wire1.endTransmission();
            if (wireError == 0) {
                return true;
            }

            if (logInit) {
                LOGW("SCD4x I2C probe failed (attempt %u/%u, Wire error=%u)",
                     attempt,
                     kI2cProbeAttempts,
                     wireError);
            }

            resetScd4xWireBus();
            vTaskDelay(pdMS_TO_TICKS(kI2cRetryDelayMs));
        }

        return false;
    }

    int16_t stopPeriodicMeasurementWithRetry(SensirionI2cScd4x& scd4x, bool logInit) {
        int16_t lastError = 0;

        for (uint8_t attempt = 1; attempt <= kStopPeriodicAttempts; ++attempt) {
            lastError = scd4x.stopPeriodicMeasurement();
            if (lastError == 0) {
                return 0;
            }

            if (logInit) {
                LOGW("stopPeriodicMeasurement failed (attempt %u/%u): %d (%s)",
                     attempt,
                     kStopPeriodicAttempts,
                     lastError,
                     errorCodeToString(lastError));
            }

            resetScd4xWireBus();
            scd4x.begin(Wire1, I2C_SCD4X::I2C_ADDRESS);
            vTaskDelay(pdMS_TO_TICKS(kI2cRetryDelayMs));
        }

        return lastError;
    }

    bool shouldLogThrottled(uint32_t nowMs, uint32_t& lastLogMs, uint32_t intervalMs, uint32_t& suppressed) {
        if (lastLogMs == 0 || nowMs - lastLogMs >= intervalMs) {
            lastLogMs = nowMs;
            return true;
        }

        if (suppressed < UINT32_MAX) {
            ++suppressed;
        }
        return false;
    }

    uint32_t consumeSuppressed(uint32_t& suppressed) {
        const uint32_t value = suppressed;
        suppressed = 0;
        return value;
    }

    uint32_t g_lastSelfHealInitLogMs = 0;
    uint32_t g_suppressedSelfHealInitLogs = 0;
}  // namespace

Scd4xSensorService::Scd4xSensorService(COMPENSATION::CompensationService* compensationService) 
    : _initialized(false), _sensorPresent(false), _compensationService(compensationService) {
}

bool Scd4xSensorService::begin() {
    if (_initialized) {
        return true;
    }

    if (!_compensationService) {
        LOGW("Compensation service not injected!");
    }

    const bool logInit = !_selfHealingReinit ||
        shouldLogThrottled(
            millis(),
            g_lastSelfHealInitLogMs,
            SENSOR::TASK_LOOP::SELF_HEAL_LOG_INTERVAL_MS,
            g_suppressedSelfHealInitLogs);

    if (logInit) {
        const uint32_t suppressed = consumeSuppressed(g_suppressedSelfHealInitLogs);
        if (_selfHealingReinit && suppressed > 0) {
            LOGW("SCD4x self-heal init retry (suppressed %lu attempts)",
                 static_cast<unsigned long>(suppressed));
        }
        LOGW("Initializing I2C_SCD4X (Wire1) on SCL=%d, SDA=%d", I2C_SCD4X::SCL_PIN, I2C_SCD4X::SDA_PIN);
    }

    resetScd4xWireBus();
    
    // SCD4x needs at least 1000ms after power-on before it can respond to I2C
    if (logInit) {
        LOGI("Waiting for SCD4x power-up (%dms)...", SENSOR::SCD4X::POWER_UP_DELAY_MS);
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR::SCD4X::POWER_UP_DELAY_MS));
    
    _scd4x.begin(Wire1, I2C_SCD4X::I2C_ADDRESS);

    if (!probeScd4xAddress(logInit)) {
        if (logInit) {
            LOGW("SCD4x I2C address 0x%02X not detected. Marking as missing.", I2C_SCD4X::I2C_ADDRESS);
        }
        _sensorPresent = false;
        _initialized = true;
        return true;
    }

    int16_t error = stopPeriodicMeasurementWithRetry(_scd4x, logInit);
    if (error != 0) {
        if (logInit) {
            LOGW("SCD4x init failed after stopPeriodicMeasurement retries. Marking as missing.");
        }
        _sensorPresent = false;
        _initialized = true;
        return true;
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR::SCD4X::STOP_PERIODIC_DELAY_MS));

    // Reinitialize sensor
    error = _scd4x.reinit();
    if (error != 0) {
        LOGE("reinit failed: %d (%s)", error, errorCodeToString(error));
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR::SCD4X::REINIT_DELAY_MS));

    // Read and log serial number
    uint64_t serialNumber = 0;
    error = _scd4x.getSerialNumber(serialNumber);
    if (error != 0) {
        LOGE("getSerialNumber failed: %d (%s)", error, errorCodeToString(error));
        return false;
    }
    LOGI("Serial: 0x%08X%08X", (uint32_t)(serialNumber >> 32), (uint32_t)(serialNumber & 0xFFFFFFFF));

    // Read current hardware temperature offset from EEPROM
    float hwTempOffset = 0.0f;
    error = _scd4x.getTemperatureOffset(hwTempOffset);
    if (error != 0) {
        LOGW("getTemperatureOffset failed: %d (%s)", error, errorCodeToString(error));
    } else {
        LOGI("Hardware temp offset (EEPROM): %.2f C", hwTempOffset);
    }

    // Start periodic measurement (5 second interval)
    error = _scd4x.startPeriodicMeasurement();
    if (error != 0) {
        LOGE("startPeriodicMeasurement failed: %d (%s)", error, errorCodeToString(error));
        _sensorPresent = false;
        _initialized = true; 
        return true; 
    }

    _sensorPresent = true;
    _initialized = true;
    LOGI("Initialized successfully");
    return true;
}

bool Scd4xSensorService::reinitialize() {
    _initialized = false;
    _sensorPresent = false;
    _selfHealingReinit = true;
    const bool initialized = begin();
    _selfHealingReinit = false;
    return initialized && _sensorPresent;
}

void Scd4xSensorService::readAll(SensorSnapshot& outSnap, PhaseStatus& outStatus) {
    outStatus.start_ms = millis();
    outStatus.ok = false;
    outStatus.error_code = nullptr;
    outStatus.error_detail = nullptr;

    if (!_initialized) {
        outStatus.error_code = "NOT_INIT";
        outStatus.error_detail = "Call begin() first";
        outStatus.duration_ms = millis() - outStatus.start_ms;
        return;
    }

    if (!_sensorPresent) {
        outStatus.error_code = "SENSOR_MISSING";
        outStatus.error_detail = "SCD4x not detected at startup";
        outStatus.duration_ms = millis() - outStatus.start_ms;
        return;
    }

    bool dataReady = false;
    int16_t error = _scd4x.getDataReadyStatus(dataReady);
    if (error != 0) {
        outStatus.error_code = "DATA_READY_ERR";
        outStatus.error_detail = errorCodeToString(error);
        outStatus.duration_ms = millis() - outStatus.start_ms;
        LOGE("getDataReadyStatus failed: %d", error);
        return;
    }

    if (!dataReady) {
        outStatus.error_code = "NO_DATA";
        outStatus.error_detail = "Measurement not ready yet";
        outStatus.duration_ms = millis() - outStatus.start_ms;
        return;
    }

    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    
    error = _scd4x.readMeasurement(co2, temperature, humidity);
    if (error != 0) {
        outStatus.error_code = "READ_ERR";
        outStatus.error_detail = errorCodeToString(error);
        outStatus.duration_ms = millis() - outStatus.start_ms;
        LOGE("readMeasurement failed: %d", error);
        return;
    }

    if (co2 < SENSOR::SCD4X::CO2_MIN_PPM || co2 > SENSOR::SCD4X::CO2_MAX_PPM ||
        temperature < SENSOR::SCD4X::TEMP_MIN_C || temperature > SENSOR::SCD4X::TEMP_MAX_C ||
        humidity < SENSOR::SCD4X::HUMID_MIN_PCT || humidity > SENSOR::SCD4X::HUMID_MAX_PCT) {
        outStatus.error_code = "INVALID_DATA";
        outStatus.ok = false;
        outStatus.duration_ms = millis() - outStatus.start_ms;
        LOGW("Invalid sensor data (out of range): CO2=%u ppm (min=%u max=%u), Temp=%.2f C (min=%.2f max=%.2f), Humid=%.2f %% (min=%.2f max=%.2f)",
             co2,
             SENSOR::SCD4X::CO2_MIN_PPM,
             SENSOR::SCD4X::CO2_MAX_PPM,
             temperature,
             SENSOR::SCD4X::TEMP_MIN_C,
             SENSOR::SCD4X::TEMP_MAX_C,
             humidity,
             SENSOR::SCD4X::HUMID_MIN_PCT,
             SENSOR::SCD4X::HUMID_MAX_PCT);
        return;
    }

    float compTemp = temperature;
    float compHumid = humidity;

    if (_compensationService) {
        auto compensated = _compensationService->compensate(temperature, humidity);
        compTemp = compensated.temperature;
        compHumid = compensated.humidity;
    }

    outSnap.co2 = co2;
    outSnap.temp = compTemp;
    outSnap.humid = compHumid;
    outSnap.timestamp_ms = millis();

    outStatus.duration_ms = millis() - outStatus.start_ms;
    outStatus.ok = true;

    LOGD_THROTTLED(TASK_MONITOR::INTERVAL_I2C_READ_MS,
                   "CO2=%u ppm, T=%.1f C, RH=%.1f%%",
                   co2,
                   temperature,
                   humidity);
}

void Scd4xSensorService::sleep() {
    if (!_initialized) return;

    int16_t error = _scd4x.stopPeriodicMeasurement();
    if (error != 0) {
        LOGW("stopPeriodicMeasurement failed: %d", error);
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR::SCD4X::STOP_PERIODIC_DELAY_MS));

    error = _scd4x.powerDown();
    if (error != 0) {
        LOGW("powerDown failed: %d", error);
    }

    LOGI("Sensor in sleep mode");
}

void Scd4xSensorService::wake() {
    if (!_initialized) return;

    int16_t error = _scd4x.wakeUp();
    if (error != 0) {
        LOGW("wakeUp failed: %d", error);
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR::SCD4X::REINIT_DELAY_MS));

    error = _scd4x.startPeriodicMeasurement();
    if (error != 0) {
        LOGE("startPeriodicMeasurement failed: %d", error);
        return;
    }

    LOGI("Sensor awake, measurements started");
}

}  // namespace SENSORS
