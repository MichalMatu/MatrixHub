#include "CompensationService.h"
#include "../config/System.h"
#include "../system/rtc/RtcConfig.h"
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "CompService"
#include "../system/logging/Logging.h"

namespace COMPENSATION {

namespace {
    inline float clamp(float value, float lo, float hi) {
        return fminf(fmaxf(value, lo), hi);
    }
}

CompensationService::CompensationService()
    : m_cachedConfig(RTC::CompensationData{}),
      m_logCounter(0),
      m_filteredCpuTemp(CONFIG::COMPENSATION::CPU_TEMP_FALLBACK_C),
      m_filterInitialized(false) {}

void CompensationService::begin() {
    applySettings();
    LOGI("Service initialized");
}

void CompensationService::applySettings() {
    RTC::CompensationData cfg{};
    bool got = false;
    RTC::withConfig([&](const RTC::ConfigStore& store) {
        cfg = store.compensation;
        got = true;
    });
    if (!got) {
        LOGW("applySettings: RTC lock timeout, keeping cached config");
        return;
    }

    portENTER_CRITICAL(&m_cfgMux);
    m_cachedConfig = cfg;
    portEXIT_CRITICAL(&m_cfgMux);
}

RTC::CompensationData CompensationService::getSettings() const {
    portENTER_CRITICAL(&m_cfgMux);
    const RTC::CompensationData cfg = m_cachedConfig;
    portEXIT_CRITICAL(&m_cfgMux);
    return cfg;
}

void CompensationService::resetToDefaults() {
    updateSettings(RTC::CompensationData{});
    LOGI("Reset to factory defaults");
}

bool CompensationService::updateSettings(const RTC::CompensationData& config) {
    // Validate on local copy
    RTC::CompensationData validated = config;
    validate(validated);

    // Write to RTC
    const bool updated = RTC::updateConfig([validated](RTC::ConfigStore& store) {
        store.compensation = validated;
    });
    if (!updated) {
        LOGE("Failed to update RTC settings (mutex timeout)");
        return false;
    }

    // Update cache
    portENTER_CRITICAL(&m_cfgMux);
    m_cachedConfig = validated;
    portEXIT_CRITICAL(&m_cfgMux);

    // Note: To persist to file, the caller (API) must trigger ConfigManager::save()
    // This is the standard pattern (API service handles config save triggering if needed)

    LOGI("Updated: en=%d base=%.1f ref=%.1f slope=%.2f range=[%.1f,%.1f]",
         validated.enabled, validated.baseTempOffset, validated.referenceCpuTemp,
         validated.tempOffsetPerCpuDegree, validated.minTempOffset, validated.maxTempOffset);

    return true;
}

void CompensationService::validate(RTC::CompensationData& cfg) {
    using namespace LIMITS::COMPENSATION;
    cfg.baseTempOffset         = clamp(cfg.baseTempOffset, MIN_BASE_OFFSET, MAX_BASE_OFFSET);
    cfg.referenceCpuTemp       = clamp(cfg.referenceCpuTemp, MIN_REF_CPU_TEMP, MAX_REF_CPU_TEMP);
    cfg.tempOffsetPerCpuDegree = clamp(cfg.tempOffsetPerCpuDegree, MIN_SLOPE, MAX_SLOPE);
    cfg.minTempOffset          = clamp(cfg.minTempOffset, MIN_OFFSET_CLAMP, MAX_OFFSET_CLAMP);
    cfg.maxTempOffset          = clamp(cfg.maxTempOffset, MIN_OFFSET_CLAMP, MAX_OFFSET_CLAMP);

    if (cfg.minTempOffset > cfg.maxTempOffset) {
        cfg.minTempOffset = cfg.maxTempOffset;
    }
}

// =============================================================================
// Business Logic (Compensation Math)
// =============================================================================

float CompensationService::getCpuTemperature() {
#if defined(ESP32)
    float rawCpuTemp = temperatureRead();
#else
    float rawCpuTemp = CONFIG::COMPENSATION::CPU_TEMP_FALLBACK_C;
#endif
    if (std::isnan(rawCpuTemp)) {
        LOGW("CPU temperature read returned NaN, using fallback=%.1f°C",
             CONFIG::COMPENSATION::CPU_TEMP_FALLBACK_C);
        rawCpuTemp = CONFIG::COMPENSATION::CPU_TEMP_FALLBACK_C;
    }

    if (!m_filterInitialized) {
        m_filteredCpuTemp = rawCpuTemp;
        m_filterInitialized = true;
        return m_filteredCpuTemp;
    }

    const float alpha = CONFIG::COMPENSATION::CPU_TEMP_EMA_ALPHA;
    m_filteredCpuTemp = (alpha * rawCpuTemp) + ((1.0f - alpha) * m_filteredCpuTemp);
    return m_filteredCpuTemp;
}

float CompensationService::calculateOffset(float cpuTemp, const RTC::CompensationData& cfg) {
    float deltaFromReference = cpuTemp - cfg.referenceCpuTemp;
    float offset = cfg.baseTempOffset + (deltaFromReference * cfg.tempOffsetPerCpuDegree);
    
    // Clamp to valid range
    return clamp(offset, cfg.minTempOffset, cfg.maxTempOffset);
}

float CompensationService::compensateHumidity(float rawRH, float rawTemp, float compTemp) {
    // Fast-path: Avoid heavy math if offset is negligible
    if (fabsf(rawTemp - compTemp) < CONFIG::COMPENSATION::HUMID_FAST_PATH_THRESHOLD) {
        return clamp(rawRH, SENSOR::SCD4X::HUMID_MIN_PCT, SENSOR::SCD4X::HUMID_MAX_PCT);
    }

    const float a = CONFIG::COMPENSATION::MAGNUS_A;
    const float b = CONFIG::COMPENSATION::MAGNUS_B;
    const float denomMin = CONFIG::COMPENSATION::MAGNUS_DENOM_MIN;
    const float denomRaw = b + rawTemp;
    const float denomComp = b + compTemp;

    if ((denomRaw <= denomMin) || (denomComp <= denomMin)) {
        LOGW("Magnus denominator too small (raw=%.3f, comp=%.3f). Returning raw RH.", denomRaw, denomComp);
        return rawRH;
    }

    float gamma_raw  = (a * rawTemp)  / denomRaw;
    float gamma_comp = (a * compTemp) / denomComp;
    
    float correctedRH = rawRH * expf(gamma_raw - gamma_comp);
    
    // Clamp to physical range
    return clamp(correctedRH, SENSOR::SCD4X::HUMID_MIN_PCT, SENSOR::SCD4X::HUMID_MAX_PCT);
}

CompensatedReading CompensationService::compensate(float rawTemp, float rawHumid) {
    // 1. Get cached config (fast, no RTC lock)
    RTC::CompensationData cfg;
    portENTER_CRITICAL(&m_cfgMux);
    cfg = m_cachedConfig;
    portEXIT_CRITICAL(&m_cfgMux);

    // 2. Master switch — pass through raw values when disabled
    if (!cfg.enabled) {
        m_filterInitialized = false;
        return { rawTemp, rawHumid, 0.0f, 0.0f, 0.0f };
    }

    // 3. Get Hardware Data (only if enabled)
    const float cpuTemp = getCpuTemperature();

    // 4. Logic
    CompensatedReading result;
    result.cpuTemp = cpuTemp;
    result.tempOffset = calculateOffset(cpuTemp, cfg);
    result.temperature = rawTemp - result.tempOffset;
    
    // Apply humidity compensation
    result.humidity = compensateHumidity(rawHumid, rawTemp, result.temperature);
    result.humidOffset = result.humidity - rawHumid;
    
    // 5. Logging (Throttled)
    if (++m_logCounter >= CONFIG::COMPENSATION::LOG_INTERVAL_TICKS) {
        LOGD("CPU=%.1f°C | Raw: T=%.1f H=%.1f | Off=%.1f | Res: T=%.1f H=%.1f",
             result.cpuTemp, rawTemp, rawHumid, 
             result.tempOffset,
             result.temperature, result.humidity);
        m_logCounter = 0;
    }
    
    return result;
}

} // namespace COMPENSATION
