#ifdef UNIT_TEST

#include <cstring>
#include <limits>

#include <unity.h>

#include "../../src/alarms/core/AlarmRuleIdentity.h"

namespace {

ALARMS::AlarmRule makeRule(const char* id,
                           ALARMS::AlarmSource source,
                           ALARMS::AlarmOperator op = ALARMS::AlarmOperator::Above,
                           float threshold = 0.0f) {
    ALARMS::AlarmRule rule;
    std::strncpy(rule.id, id, sizeof(rule.id) - 1);
    std::strncpy(rule.name, "Identity fixture", sizeof(rule.name) - 1);
    rule.enabled = true;
    rule.source = source;
    rule.op = op;
    rule.threshold = threshold;
    return rule;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_runtime_identity_hash_has_stable_golden_vectors() {
    ALARMS::AlarmRule csi =
        makeRule("csi-motion", ALARMS::AlarmSource::WifiCsiMotion);
    TEST_ASSERT_EQUAL_UINT64(0x5d5e811514c4b77eULL,
                             ALARMS::stableAlarmRuntimeIdentityHash(csi));

    ALARMS::AlarmRule temperature =
        makeRule("temperature", ALARMS::AlarmSource::Temperature,
                 ALARMS::AlarmOperator::Above, 30.0f);
    TEST_ASSERT_EQUAL_UINT64(
        0xdbd17d72bf6f16e5ULL,
        ALARMS::stableAlarmRuntimeIdentityHash(temperature));

    ALARMS::AlarmRule ble =
        makeRule("ble-temp", ALARMS::AlarmSource::BleTemperature,
                 ALARMS::AlarmOperator::Below, -5.25f);
    std::strncpy(ble.bleDeviceMac,
                 "A4:C1:38:12:34:56",
                 sizeof(ble.bleDeviceMac) - 1);
    TEST_ASSERT_EQUAL_UINT64(0xde18e1fa6fb11d74ULL,
                             ALARMS::stableAlarmRuntimeIdentityHash(ble));

    ALARMS::AlarmRule gpio =
        makeRule("door", ALARMS::AlarmSource::GpioDigital);
    std::strncpy(gpio.gpioId, "gpio38", sizeof(gpio.gpioId) - 1);
    TEST_ASSERT_EQUAL_UINT64(0x2259df956e9365d2ULL,
                             ALARMS::stableAlarmRuntimeIdentityHash(gpio));
}

void test_boolean_identity_ignores_legacy_operator_threshold_and_presentation() {
    ALARMS::AlarmRule canonical =
        makeRule("csi-motion", ALARMS::AlarmSource::WifiCsiMotion,
                 ALARMS::AlarmOperator::Above, 0.5f);
    ALARMS::AlarmRule legacy = canonical;
    legacy.op = ALARMS::AlarmOperator::Below;
    legacy.threshold = 42.0f;
    std::strncpy(legacy.name, "Renamed", sizeof(legacy.name) - 1);
    legacy.cooldownSeconds = 5;
    legacy.severity = ALARMS::AlarmSeverity::Critical;

    TEST_ASSERT_TRUE(ALARMS::hasSameAlarmRuntimeIdentity(canonical, legacy));
    TEST_ASSERT_EQUAL_UINT64(
        ALARMS::stableAlarmRuntimeIdentityHash(canonical),
        ALARMS::stableAlarmRuntimeIdentityHash(legacy));
}

void test_analog_semantic_changes_produce_distinct_runtime_identities() {
    ALARMS::AlarmRule base =
        makeRule("temperature", ALARMS::AlarmSource::Temperature,
                 ALARMS::AlarmOperator::Above, 30.0f);

    ALARMS::AlarmRule changed = base;
    changed.threshold = 31.0f;
    TEST_ASSERT_FALSE(ALARMS::hasSameAlarmRuntimeIdentity(base, changed));
    TEST_ASSERT_NOT_EQUAL(ALARMS::stableAlarmRuntimeIdentityHash(base),
                          ALARMS::stableAlarmRuntimeIdentityHash(changed));

    changed = base;
    changed.op = ALARMS::AlarmOperator::Below;
    TEST_ASSERT_FALSE(ALARMS::hasSameAlarmRuntimeIdentity(base, changed));

    changed = base;
    changed.source = ALARMS::AlarmSource::Humidity;
    TEST_ASSERT_FALSE(ALARMS::hasSameAlarmRuntimeIdentity(base, changed));

    changed = base;
    std::strncpy(changed.id, "temperature-2", sizeof(changed.id) - 1);
    TEST_ASSERT_FALSE(ALARMS::hasSameAlarmRuntimeIdentity(base, changed));
}

void test_selector_changes_produce_distinct_runtime_identities() {
    ALARMS::AlarmRule ble =
        makeRule("ble", ALARMS::AlarmSource::BleTemperature,
                 ALARMS::AlarmOperator::Above, 20.0f);
    std::strncpy(ble.bleDeviceMac,
                 "A4:C1:38:12:34:56",
                 sizeof(ble.bleDeviceMac) - 1);
    ALARMS::AlarmRule otherBle = ble;
    std::strncpy(otherBle.bleDeviceMac,
                 "A4:C1:38:12:34:57",
                 sizeof(otherBle.bleDeviceMac) - 1);
    TEST_ASSERT_FALSE(ALARMS::hasSameAlarmRuntimeIdentity(ble, otherBle));

    ALARMS::AlarmRule gpio =
        makeRule("gpio", ALARMS::AlarmSource::GpioDigital);
    std::strncpy(gpio.gpioId, "gpio38", sizeof(gpio.gpioId) - 1);
    ALARMS::AlarmRule otherGpio = gpio;
    std::strncpy(otherGpio.gpioId, "gpio39", sizeof(otherGpio.gpioId) - 1);
    TEST_ASSERT_FALSE(ALARMS::hasSameAlarmRuntimeIdentity(gpio, otherGpio));
}

void test_zero_threshold_is_canonical_and_invalid_rules_fail_closed() {
    ALARMS::AlarmRule positiveZero =
        makeRule("zero", ALARMS::AlarmSource::Temperature,
                 ALARMS::AlarmOperator::Above, 0.0f);
    ALARMS::AlarmRule negativeZero = positiveZero;
    negativeZero.threshold = -0.0f;

    TEST_ASSERT_TRUE(
        ALARMS::hasSameAlarmRuntimeIdentity(positiveZero, negativeZero));
    TEST_ASSERT_EQUAL_UINT64(
        0x50d3d1fce5554dcbULL,
        ALARMS::stableAlarmRuntimeIdentityHash(positiveZero));
    TEST_ASSERT_EQUAL_UINT64(
        ALARMS::stableAlarmRuntimeIdentityHash(positiveZero),
        ALARMS::stableAlarmRuntimeIdentityHash(negativeZero));

    ALARMS::AlarmRule disabled = positiveZero;
    disabled.enabled = false;
    TEST_ASSERT_EQUAL_UINT64(0, ALARMS::stableAlarmRuntimeIdentityHash(disabled));
    TEST_ASSERT_FALSE(
        ALARMS::hasSameAlarmRuntimeIdentity(disabled, disabled));

    ALARMS::AlarmRule nonFinite = positiveZero;
    nonFinite.threshold = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_EQUAL_UINT64(
        0, ALARMS::stableAlarmRuntimeIdentityHash(nonFinite));
    TEST_ASSERT_FALSE(
        ALARMS::hasSameAlarmRuntimeIdentity(nonFinite, nonFinite));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_runtime_identity_hash_has_stable_golden_vectors);
    RUN_TEST(test_boolean_identity_ignores_legacy_operator_threshold_and_presentation);
    RUN_TEST(test_analog_semantic_changes_produce_distinct_runtime_identities);
    RUN_TEST(test_selector_changes_produce_distinct_runtime_identities);
    RUN_TEST(test_zero_threshold_is_canonical_and_invalid_rules_fail_closed);
    return UNITY_END();
}

#endif  // UNIT_TEST
