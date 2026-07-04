#include <unity.h>
#include <cmath>
#include "../../src/system/rtc/types/RtcCompensationTypes.h"
// FreeRTOS critical section stubs for native tests
#ifndef portENTER_CRITICAL
#define portENTER_CRITICAL(mux) ((void)(mux))
#endif
#ifndef portEXIT_CRITICAL
#define portEXIT_CRITICAL(mux) ((void)(mux))
#endif
// Include implementation for native linking
#include "../../src/compensation/CompensationService.cpp" 

// --- Stubs for Linking ---
namespace LOG {
    void Logging::log(esp_log_level_t level, const char* tag, const char* format, ...) {
        // No-op for tests
    }
}

namespace RTC {
    // ConfigStore is already defined in RtcConfig.h which is included by CompensationService.cpp
    static ConfigStore mockStore;

    const ConfigStore& getConfig() {
        return mockStore;
    }

    void withConfig(const std::function<void(const ConfigStore&)>& reader) {
        reader(mockStore);
    }
    
    // Stub for getConfigSafeCopy
    ConfigStore getConfigSafeCopy() {
        return mockStore;
    }

    // Stub for updateConfig
    bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
        updater(mockStore);
        return true;
    }
}

using namespace COMPENSATION;

// ============================================================================
// Default config (matches inline defaults in RtcCompensationTypes.h)
// ============================================================================
static const RTC::CompensationData defaultCfg = RTC::CompensationData{};

void setUp(void) {
    RTC::updateConfig([](RTC::ConfigStore& store) {
        store = RTC::ConfigStore{};
    });
}
void tearDown(void) {}

// ============================================================================
// Test Cases — Default Config (regression)
// ============================================================================

void test_compensation_at_reference_temp() {
    float rawTemp = 25.0f;
    float rawHumid = 50.0f;
    float cpuTemp = defaultCfg.referenceCpuTemp; // 45.0

    // Expected: offset = BASE_OFFSET (2.0)
    // Result temp = 25.0 - 2.0 = 23.0
    
    // Testing static helper directly
    float offset = CompensationService::calculateOffset(cpuTemp, defaultCfg);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, defaultCfg.baseTempOffset, offset);
    
    float compensatedTemp = rawTemp - offset;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, compensatedTemp);

    // Humidity increases via Magnus formula (25->23C)
    float compensatedHumid = CompensationService::compensateHumidity(rawHumid, rawTemp, compensatedTemp);
    
    // Verify pure math
    // Magnus(25->23) approx 1.13x -> 50 * 1.13 = 56.4
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 56.4f, compensatedHumid);
}

void test_compensation_above_reference() {
    float rawTemp = 25.0f;
    // float rawHumid = 50.0f;
    float cpuTemp = defaultCfg.referenceCpuTemp + 10.0f; // 55.0

    // Expected: offset = BASE + (10 * 0.2) = 2.0 + 2.0 = 4.0
    // Result temp = 25.0 - 4.0 = 21.0
    
    float offset = CompensationService::calculateOffset(cpuTemp, defaultCfg);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, offset);
}

void test_compensation_below_reference() {
    float rawTemp = 25.0f;
    // float rawHumid = 50.0f;
    float cpuTemp = defaultCfg.referenceCpuTemp - 10.0f; // 35.0

    // Expected: offset = BASE + (-10 * 0.2) = 2.0 - 2.0 = 0.0
    // Result temp = 25.0 - 0.0 = 25.0
    
    float offset = CompensationService::calculateOffset(cpuTemp, defaultCfg);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, offset);
}

void test_clamping_min() {
    // Force offset below 0
    float cpuTemp = defaultCfg.referenceCpuTemp - 30.0f; // 15.0

    // Raw offset: 2.0 + (-30 * 0.2) = 2.0 - 6.0 = -4.0
    // Clamped: 0.0 (default min)
    
    float offset = CompensationService::calculateOffset(cpuTemp, defaultCfg);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, defaultCfg.minTempOffset, offset);
}

void test_clamping_max() {
    // Force offset above 15
    float cpuTemp = defaultCfg.referenceCpuTemp + 50.0f; // 95.0

    // Raw offset: 2.0 + (50 * 0.2) = 2.0 + 10.0 = 12.0
    // Clamped: 8.0
    
    float offset = CompensationService::calculateOffset(cpuTemp, defaultCfg);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, defaultCfg.maxTempOffset, offset);
}

void test_humidity_compensated_at_reference() {
    // At reference CPU temp, offset = baseTempOffset (2.0)
    // rawTemp=25, compTemp=23 -> Magnus increases RH
    
    float rawTemp = 25.0f;
    float rawHumid = 50.0f;
    float compTemp = rawTemp - defaultCfg.baseTempOffset;

    float result = CompensationService::compensateHumidity(rawHumid, rawTemp, compTemp);
    
    // Humidity should increase when temp is corrected downward
    TEST_ASSERT_TRUE(result > 50.0f);
}

// ============================================================================
// Test Cases — Custom Config
// ============================================================================

void test_custom_config_different_base() {
    RTC::CompensationData cfg = defaultCfg;
    cfg.baseTempOffset = 3.0f;
    cfg.referenceCpuTemp = 45.0f;
    cfg.tempOffsetPerCpuDegree = 0.2f;
    cfg.minTempOffset = 0.5f;
    cfg.maxTempOffset = 10.0f;

    // At reference temp: offset = 3.0
    float offset = CompensationService::calculateOffset(45.0f, cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, offset);

    // 10C above reference: offset = 3.0 + (10 * 0.2) = 5.0
    offset = CompensationService::calculateOffset(55.0f, cfg);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, offset);
}

void test_custom_config_clamping() {
    RTC::CompensationData cfg = defaultCfg;
    cfg.baseTempOffset = 2.0f;
    cfg.referenceCpuTemp = 40.0f;
    cfg.tempOffsetPerCpuDegree = 0.5f;
    cfg.minTempOffset = 1.0f;   // Higher min
    cfg.maxTempOffset = 8.0f;   // Lower max

    // Very cold CPU -> should clamp to min (1.0)
    float offset = CompensationService::calculateOffset(20.0f, cfg);
    // Raw: 2.0 + (-20 * 0.5) = -8.0 -> clamped to 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, offset);

    // Very hot CPU -> should clamp to max (8.0)
    offset = CompensationService::calculateOffset(80.0f, cfg);
    // Raw: 2.0 + (40 * 0.5) = 22.0 -> clamped to 8.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.0f, offset);
}

void test_custom_config_zero_slope() {
    RTC::CompensationData cfg = defaultCfg;
    cfg.baseTempOffset = 4.0f;
    cfg.referenceCpuTemp = 40.0f;
    cfg.tempOffsetPerCpuDegree = 0.0f; // Flat — offset always = base
    cfg.minTempOffset = 0.0f;
    cfg.maxTempOffset = 15.0f;

    // Any CPU temp should give base offset
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, CompensationService::calculateOffset(20.0f, cfg));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, CompensationService::calculateOffset(60.0f, cfg));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, CompensationService::calculateOffset(80.0f, cfg));
}

// ============================================================================
// Test Cases — compensateHumidity (Magnus formula)
// ============================================================================

void test_magnus_no_change_when_temps_equal() {
    // When rawTemp == compTemp, gamma difference is 0, exp(0) = 1 -> RH unchanged
    float result = CompensationService::compensateHumidity(50.0f, 25.0f, 25.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result);
}

void test_magnus_fast_path_threshold() {
    // Very small delta should use fast-path and preserve RH
    float result = CompensationService::compensateHumidity(50.0f, 25.0f, 25.005f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result);
}

void test_magnus_rh_increases_when_temp_drops() {
    // Cooling from 25C to 20C -> RH must increase
    float result = CompensationService::compensateHumidity(50.0f, 25.0f, 20.0f);
    TEST_ASSERT_TRUE(result > 50.0f);
    // Magnus: 50% RH at 25->20C approx 67.7%
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 67.7f, result);
}

void test_magnus_clamp_to_100() {
    // Very high RH with large temp drop -> should clamp to 100%
    float result = CompensationService::compensateHumidity(95.0f, 30.0f, 15.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, result);
}

void test_magnus_clamp_to_0() {
    // 0% RH stays at 0% regardless of temp correction
    float result = CompensationService::compensateHumidity(0.0f, 25.0f, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, result);
}

void test_magnus_negative_temperatures() {
    // Edge case: sub-zero temps (e.g. outdoor sensor in winter)
    // -10C -> -15C: RH should still increase when temp drops
    float result = CompensationService::compensateHumidity(50.0f, -10.0f, -15.0f);
    TEST_ASSERT_TRUE(result > 50.0f);
    TEST_ASSERT_TRUE(result < 100.0f);
    
    // Same temp -> no change
    float same = CompensationService::compensateHumidity(50.0f, -10.0f, -10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, same);
}

void test_magnus_precision_at_typical_values() {
    // Verify known Magnus result: 50%RH at 25C -> 20C
    float result = CompensationService::compensateHumidity(50.0f, 25.0f, 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 67.7f, result);
}

void test_magnus_denominator_guard() {
    // Denominator too small should fall back to raw RH
    float rawRh = 42.0f;
    float rawTemp = -243.119f; // b + rawTemp ~= 0.001
    float compTemp = -243.0f;
    float result = CompensationService::compensateHumidity(rawRh, rawTemp, compTemp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, rawRh, result);
}

// ============================================================================
// Test Cases — Service Cache / RTC Consistency + Disabled Fast-Path
// ============================================================================

void test_service_applySettings_copies_rtc_to_cache() {
    RTC::CompensationData cfg = defaultCfg;
    cfg.enabled = true;
    cfg.baseTempOffset = 3.5f;
    cfg.referenceCpuTemp = 50.0f;
    cfg.tempOffsetPerCpuDegree = 0.25f;
    cfg.minTempOffset = 0.5f;
    cfg.maxTempOffset = 9.5f;

    RTC::updateConfig([&](RTC::ConfigStore& store) {
        store.compensation = cfg;
    });

    CompensationService service;
    service.applySettings();

    const auto cached = service.getSettings();
    TEST_ASSERT_EQUAL(cfg.enabled, cached.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg.baseTempOffset, cached.baseTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg.referenceCpuTemp, cached.referenceCpuTemp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg.tempOffsetPerCpuDegree, cached.tempOffsetPerCpuDegree);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg.minTempOffset, cached.minTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg.maxTempOffset, cached.maxTempOffset);
}

void test_service_updateSettings_updates_rtc_and_cache() {
    RTC::CompensationData cfg = defaultCfg;
    cfg.enabled = true;
    cfg.baseTempOffset = 30.0f;     // should clamp to 20.0 (MAX_BASE_OFFSET)
    cfg.referenceCpuTemp = 10.0f;  // should clamp to 20.0 (MIN_REF_CPU_TEMP)
    cfg.tempOffsetPerCpuDegree = 3.0f; // should clamp to 2.0 (MAX_SLOPE)
    cfg.minTempOffset = -20.0f;    // should clamp to -10.0 (MIN_OFFSET_CLAMP)
    cfg.maxTempOffset = 30.0f;     // should clamp to 25.0 (MAX_OFFSET_CLAMP)

    CompensationService service;
    bool ok = service.updateSettings(cfg);
    TEST_ASSERT_TRUE(ok);

    const auto rtc = RTC::getConfig().compensation;
    const auto cached = service.getSettings();

    TEST_ASSERT_EQUAL(true, rtc.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, LIMITS::COMPENSATION::MAX_BASE_OFFSET, rtc.baseTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, LIMITS::COMPENSATION::MIN_REF_CPU_TEMP, rtc.referenceCpuTemp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, LIMITS::COMPENSATION::MAX_SLOPE, rtc.tempOffsetPerCpuDegree);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, LIMITS::COMPENSATION::MIN_OFFSET_CLAMP, rtc.minTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, LIMITS::COMPENSATION::MAX_OFFSET_CLAMP, rtc.maxTempOffset);

    TEST_ASSERT_EQUAL(rtc.enabled, cached.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, rtc.baseTempOffset, cached.baseTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, rtc.referenceCpuTemp, cached.referenceCpuTemp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, rtc.tempOffsetPerCpuDegree, cached.tempOffsetPerCpuDegree);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, rtc.minTempOffset, cached.minTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, rtc.maxTempOffset, cached.maxTempOffset);
}

void test_compensate_disabled_passthrough() {
    RTC::CompensationData cfg = defaultCfg;
    cfg.enabled = false;
    cfg.baseTempOffset = 6.0f;
    cfg.tempOffsetPerCpuDegree = 0.5f;
    cfg.referenceCpuTemp = 55.0f;
    cfg.minTempOffset = 1.0f;
    cfg.maxTempOffset = 12.0f;

    CompensationService service;
    TEST_ASSERT_TRUE(service.updateSettings(cfg));

    float rawTemp = 26.5f;
    float rawHumid = 48.2f;
    auto result = service.compensate(rawTemp, rawHumid);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, rawTemp, result.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, rawHumid, result.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result.cpuTemp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result.tempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result.humidOffset);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Default config regression tests
    RUN_TEST(test_compensation_at_reference_temp);
    RUN_TEST(test_compensation_above_reference);
    RUN_TEST(test_compensation_below_reference);
    RUN_TEST(test_clamping_min);
    RUN_TEST(test_clamping_max);
    RUN_TEST(test_humidity_compensated_at_reference);
    
    // Custom config tests
    RUN_TEST(test_custom_config_different_base);
    RUN_TEST(test_custom_config_clamping);
    RUN_TEST(test_custom_config_zero_slope);
    
    // Magnus formula tests
    RUN_TEST(test_magnus_no_change_when_temps_equal);
    RUN_TEST(test_magnus_fast_path_threshold);
    RUN_TEST(test_magnus_rh_increases_when_temp_drops);
    RUN_TEST(test_magnus_clamp_to_100);
    RUN_TEST(test_magnus_clamp_to_0);
    RUN_TEST(test_magnus_negative_temperatures);
    RUN_TEST(test_magnus_precision_at_typical_values);
    RUN_TEST(test_magnus_denominator_guard);
    
    // Service cache / RTC consistency + disabled passthrough
    RUN_TEST(test_service_applySettings_copies_rtc_to_cache);
    RUN_TEST(test_service_updateSettings_updates_rtc_and_cache);
    RUN_TEST(test_compensate_disabled_passthrough);
    
    return UNITY_END();
}
