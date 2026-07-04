#pragma once

#include "../system/rtc/types/RtcCompensationTypes.h"
#include <cstdint>
#include <freertos/FreeRTOS.h>

namespace COMPENSATION {

struct CompensatedReading {
    float temperature;  // Compensated temperature in °C
    float humidity;     // Compensated humidity in %
    float cpuTemp;      // CPU temperature used for calculation
    float tempOffset;   // Applied temperature offset
    float humidOffset;  // Applied humidity offset
};

/**
 * @brief Service for SCD4x temperature compensation
 * 
 * Unifies configuration management (RTC/NVS) and business logic (Compensation math).
 * Replaces legacy static classes: CompensationSettings, TemperatureCompensation.
 */
class CompensationService {
public:
    CompensationService();
    ~CompensationService() = default;

    /**
     * @brief Initialize service (load config, open NVS)
     */
    void begin();

    /**
     * @brief Pull settings from RTC and update cached config.
     */
    void applySettings();

    /**
     * @brief Get current configuration (thread-safe copy from cache)
     */
    RTC::CompensationData getSettings() const;

    /**
     * @brief Update configuration
     * Validates inputs, updates RTC, and persists to NVS backup.
     * 
     * @param config New configuration to apply
     * @return true if RTC update succeeded
     */
    bool updateSettings(const RTC::CompensationData& config);

    /**
     * @brief Reset settings to factory defaults
     */
    void resetToDefaults();

    /**
     * @brief Apply compensation to raw sensor readings
     * 
     * @param rawTemp Raw temperature from SCD4x in °C
     * @param rawHumid Raw humidity from SCD4x in %
     * @return CompensatedReading struct with final values and debug info
     */
    CompensatedReading compensate(float rawTemp, float rawHumid);



    // --- Helpers (Public Static for Testing) ---
    static float calculateOffset(float cpuTemp, const RTC::CompensationData& cfg);
    static float compensateHumidity(float rawRH, float rawTemp, float compTemp);

private:
    // --- Internal Helpers ---
    float getCpuTemperature();
    void validate(RTC::CompensationData& cfg);

    RTC::CompensationData m_cachedConfig;
    mutable portMUX_TYPE m_cfgMux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t m_logCounter;
    float m_filteredCpuTemp;
    bool m_filterInitialized;
};

} // namespace COMPENSATION
