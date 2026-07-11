/**
 * @file test_alarm_evaluator.cpp
 * @brief Unit tests for AlarmEvaluator stateless logic
 */

#include <unity.h>
#include <cmath>
#include "../../src/alarms/engine/AlarmEvaluator.h"

using namespace ALARMS;

// ============================================================================
// Helpers
// ============================================================================

AlarmRule createRule(AlarmSource source, AlarmOperator op, float threshold, uint16_t cooldown = 60) {
    AlarmRule rule;
    memset(&rule, 0, sizeof(rule));
    rule.enabled = true;
    rule.source = source;
    rule.op = op;
    rule.threshold = threshold;
    rule.cooldownSeconds = cooldown;
    return rule;
}

AlarmInputData createData(float temp) {
    AlarmInputData data;
    data.temperature = temp;
    return data;
}

// ============================================================================
// Tests
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

void test_threshold_above() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Under threshold
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(29.0f), state, 1000);
    TEST_ASSERT_FALSE(res1.triggered);
    TEST_ASSERT_FALSE(res1.shouldNotify);

    // Exact threshold (should not trigger for >)
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(30.0f), state, 2000);
    TEST_ASSERT_FALSE(res2.triggered);

    // Over threshold
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(30.1f), state, 3000);
    TEST_ASSERT_TRUE(res3.triggered);
    TEST_ASSERT_TRUE(res3.shouldNotify);
}

void test_threshold_below() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Below, 10.0f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Above threshold
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(11.0f), state, 1000);
    TEST_ASSERT_FALSE(res1.triggered);

    // Exact threshold (should not trigger for <)
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(10.0f), state, 2000);
    TEST_ASSERT_FALSE(res2.triggered);

    // Below threshold
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(9.9f), state, 3000);
    TEST_ASSERT_TRUE(res3.triggered);
}

void test_nan_handling() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Input is NaN
    EvaluationResult res = AlarmEvaluator::evaluate(rule, createData(NAN), state, 1000);
    
    TEST_ASSERT_FALSE(res.triggered);
    TEST_ASSERT_FALSE(res.stateChanged);
    TEST_ASSERT_TRUE(std::isnan(res.currentValue));
}

void test_ble_battery_source_value() {
    AlarmRule rule = createRule(AlarmSource::BleBattery, AlarmOperator::Below, 25.0f);
    AlarmInputData data;
    data.bleBattery = 20.0f;
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult res = AlarmEvaluator::evaluate(rule, data, state, 1000);

    TEST_ASSERT_TRUE(res.triggered);
    TEST_ASSERT_TRUE(res.shouldNotify);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, res.currentValue);
}

void test_ble_rssi_source_value() {
    AlarmRule rule = createRule(AlarmSource::BleRssi, AlarmOperator::Below, -90.0f);
    AlarmInputData data;
    data.bleRssi = -95.0f;
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult res = AlarmEvaluator::evaluate(rule, data, state, 1000);

    TEST_ASSERT_TRUE(res.triggered);
    TEST_ASSERT_TRUE(res.shouldNotify);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -95.0f, res.currentValue);
}

void test_wifi_csi_motion_triggers_above_half_when_detected() {
    AlarmRule rule = createRule(AlarmSource::WifiCsiMotion, AlarmOperator::Above, 0.5f);
    AlarmInputData data;
    data.wifiCsiMotion = 1.0f;
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult res = AlarmEvaluator::evaluate(rule, data, state, 1000);

    TEST_ASSERT_TRUE(res.triggered);
    TEST_ASSERT_TRUE(res.shouldNotify);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, res.currentValue);
}

void test_wifi_csi_motion_does_not_trigger_when_clear() {
    AlarmRule rule = createRule(AlarmSource::WifiCsiMotion, AlarmOperator::Above, 0.5f);
    AlarmInputData data;
    data.wifiCsiMotion = 0.0f;
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult res = AlarmEvaluator::evaluate(rule, data, state, 1000);

    TEST_ASSERT_FALSE(res.triggered);
    TEST_ASSERT_FALSE(res.shouldNotify);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, res.currentValue);
}

void test_wifi_csi_motion_cannot_be_inverted_by_stale_rule_operator() {
    AlarmRule rule = createRule(AlarmSource::WifiCsiMotion, AlarmOperator::Below, 0.5f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    AlarmInputData detected;
    detected.wifiCsiMotion = 1.0f;
    EvaluationResult result = AlarmEvaluator::evaluate(rule, detected, state, 1000);
    TEST_ASSERT_TRUE(result.triggered);

    AlarmInputData clear;
    clear.wifiCsiMotion = 0.0f;
    result = AlarmEvaluator::evaluate(rule, clear, state, 2000);
    TEST_ASSERT_FALSE(result.triggered);
    TEST_ASSERT_TRUE(result.stateChanged);
}

void test_imu_tamper_triggers_above_half_when_detected() {
    AlarmRule rule = createRule(AlarmSource::ImuTamper, AlarmOperator::Above, 0.5f);
    AlarmInputData data;
    data.imuTamper = 1.0f;
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult res = AlarmEvaluator::evaluate(rule, data, state, 1000);

    TEST_ASSERT_TRUE(res.triggered);
    TEST_ASSERT_TRUE(res.shouldNotify);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, res.currentValue);
}

void test_gpio_digital_triggers_above_half_when_true() {
    AlarmRule rule = createRule(AlarmSource::GpioDigital, AlarmOperator::Above, 0.5f);
    AlarmInputData data;
    data.gpioDigital = 1.0f;
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult res = AlarmEvaluator::evaluate(rule, data, state, 1000);

    TEST_ASSERT_TRUE(res.triggered);
    TEST_ASSERT_TRUE(res.shouldNotify);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, res.currentValue);
}

void test_state_change_detection() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // 1. Not triggered -> Triggered
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 1000);
    TEST_ASSERT_TRUE(res1.triggered);
    TEST_ASSERT_TRUE(res1.stateChanged);
    TEST_ASSERT_TRUE(state.previouslyTriggered);

    // 2. Triggered -> Triggered (No change)
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(32.0f), state, 2000);
    TEST_ASSERT_TRUE(res2.triggered);
    TEST_ASSERT_FALSE(res2.stateChanged);

    // 3. Triggered -> Not Triggered (Cleared)
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(25.0f), state, 3000);
    TEST_ASSERT_FALSE(res3.triggered);
    TEST_ASSERT_TRUE(res3.stateChanged);
    TEST_ASSERT_FALSE(state.previouslyTriggered);
}

void test_cooldown_logic() {
    // Cooldown 60 seconds (60,000 ms)
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // 1. Initial trigger -> Notify
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 100000);
    TEST_ASSERT_TRUE(res1.triggered);
    TEST_ASSERT_TRUE(res1.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(100000, state.lastTriggeredMs);

    // 2. Still triggered, 30s elapsed (Under cooldown) -> No Notify
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 130000);
    TEST_ASSERT_TRUE(res2.triggered);
    TEST_ASSERT_FALSE(res2.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(100000, state.lastTriggeredMs); // Last trigger time unchanged

    // 3. Still triggered, 61s elapsed from last trigger -> Notify Again
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 161000);
    TEST_ASSERT_TRUE(res3.triggered);
    TEST_ASSERT_TRUE(res3.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(161000, state.lastTriggeredMs); // Updated
}

void test_retained_active_alarm_anchors_new_boot_epoch_without_reminder() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // AlarmRuleManager preserves the stable active state across deep sleep,
    // but clears the old boot's millis()-based timestamp.
    state.previouslyTriggered = true;
    state.initialized = true;
    state.lastTriggeredMs = 0;

    EvaluationResult first = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 250);
    TEST_ASSERT_TRUE(first.triggered);
    TEST_ASSERT_FALSE(first.stateChanged);
    TEST_ASSERT_FALSE(first.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(250, state.lastTriggeredMs);

    EvaluationResult beforeBoundary =
        AlarmEvaluator::evaluate(rule, createData(31.0f), state, 60249);
    TEST_ASSERT_FALSE(beforeBoundary.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(250, state.lastTriggeredMs);

    EvaluationResult atBoundary =
        AlarmEvaluator::evaluate(rule, createData(31.0f), state, 60250);
    TEST_ASSERT_TRUE(atBoundary.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(60250, state.lastTriggeredMs);
}

void test_rising_edge_at_millis_zero_still_notifies_once() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    EvaluationResult rising = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 0);
    TEST_ASSERT_TRUE(rising.stateChanged);
    TEST_ASSERT_TRUE(rising.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(0, state.lastTriggeredMs);

    // The coordinator marks the runtime state initialized after the pass. The
    // zero timestamp must not turn the next steady-state sample into a second
    // notification.
    state.initialized = true;
    EvaluationResult steady = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 10);
    TEST_ASSERT_FALSE(steady.stateChanged);
    TEST_ASSERT_FALSE(steady.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(10, state.lastTriggeredMs);
}

void test_cooldown_reset_on_clear() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // 1. Triggered
    AlarmEvaluator::evaluate(rule, createData(31.0f), state, 1000);
    TEST_ASSERT_EQUAL(1000, state.lastTriggeredMs);

    // 2. Cleared
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(25.0f), state, 5000);
    TEST_ASSERT_FALSE(res2.triggered);
    TEST_ASSERT_EQUAL(0, state.lastTriggeredMs); // Should reset

    // 3. Triggered again immediately (should notify because cooldown was reset)
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 6000);
    TEST_ASSERT_TRUE(res3.triggered);
    TEST_ASSERT_TRUE(res3.shouldNotify);
}

void test_millis_overflow_cooldown() {
    // Cooldown 10ms for easy testing
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 0); // 0s in struct -> check logic manually or use small diff
    // The evaluator uses cooldownSeconds * 1000. Let's use 1s = 1000ms.
    rule.cooldownSeconds = 1; 

    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Near max uint32
    uint32_t nearMax = 0xFFFFFFF0; // 16 ms before overflow
    
    // 1. Trigger just before overflow
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, nearMax);
    TEST_ASSERT_TRUE(res1.shouldNotify);
    
    // 2. Overflow occurred, now is 100 (diff is ~116ms) - Under cooldown (1000ms)
    uint32_t afterOverflow = 100;
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, afterOverflow);
    TEST_ASSERT_FALSE(res2.shouldNotify);

    // 3. Wait until > 1000ms passed (nearMax + 1000 + 50)
    // 1000 - 16 = 984 -> So around 990 ms
    uint32_t timePassed = 2000; 
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, timePassed);
    TEST_ASSERT_TRUE(res3.shouldNotify);
}

void test_cooldown_exact_boundary_after_millis_wrap() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 1);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    constexpr uint32_t beforeWrap = 0xFFFFFF00;
    EvaluationResult first = AlarmEvaluator::evaluate(rule, createData(31.0f), state, beforeWrap);
    TEST_ASSERT_TRUE(first.shouldNotify);

    // 256 ms elapse before millis() reaches zero. Values 743 and 744 are
    // therefore exactly 999 ms and 1000 ms after the first notification.
    EvaluationResult beforeBoundary =
        AlarmEvaluator::evaluate(rule, createData(31.0f), state, 743);
    TEST_ASSERT_FALSE(beforeBoundary.shouldNotify);

    EvaluationResult atBoundary =
        AlarmEvaluator::evaluate(rule, createData(31.0f), state, 744);
    TEST_ASSERT_TRUE(atBoundary.shouldNotify);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_threshold_above);
    RUN_TEST(test_threshold_below);
    RUN_TEST(test_nan_handling);
    RUN_TEST(test_ble_battery_source_value);
    RUN_TEST(test_ble_rssi_source_value);
    RUN_TEST(test_wifi_csi_motion_triggers_above_half_when_detected);
    RUN_TEST(test_wifi_csi_motion_does_not_trigger_when_clear);
    RUN_TEST(test_wifi_csi_motion_cannot_be_inverted_by_stale_rule_operator);
    RUN_TEST(test_imu_tamper_triggers_above_half_when_detected);
    RUN_TEST(test_gpio_digital_triggers_above_half_when_true);
    RUN_TEST(test_state_change_detection);
    RUN_TEST(test_cooldown_logic);
    RUN_TEST(test_retained_active_alarm_anchors_new_boot_epoch_without_reminder);
    RUN_TEST(test_rising_edge_at_millis_zero_still_notifies_once);
    RUN_TEST(test_cooldown_reset_on_clear);
    RUN_TEST(test_millis_overflow_cooldown);
    RUN_TEST(test_cooldown_exact_boundary_after_millis_wrap);
    return UNITY_END();
}
