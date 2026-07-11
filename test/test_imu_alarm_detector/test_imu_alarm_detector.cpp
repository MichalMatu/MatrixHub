#include <unity.h>

#include "../../src/sensors/imu/ImuAlarmDetector.h"
#include "../../src/sensors/imu/ImuAlarmDetector.cpp"
#include "../../src/sensors/imu/ImuMath.cpp"

namespace {

IMU::ImuAlarmConfig config() {
    IMU::ImuAlarmConfig cfg;
    cfg.enabled = true;
    cfg.baselineValid = true;
    cfg.tiltThresholdDeg = 30.0f;
    cfg.tiltHysteresisDeg = 5.0f;
    cfg.tiltHoldMs = 500;
    cfg.tiltClearHoldMs = 1000;
    cfg.accelDeltaThresholdG = 0.35f;
    return cfg;
}

IMU::ImuMetrics metrics(float tiltDeg, float accelDeltaG = 0.0f) {
    IMU::ImuMetrics m;
    m.sampleFresh = true;
    m.sampleTimestampKnown = true;
    m.baselineValid = true;
    m.tiltDeg = tiltDeg;
    m.accelDeltaG = accelDeltaG;
    return m;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_no_baseline_reports_not_ready_without_triggering() {
    IMU::ImuAlarmDetector detector;
    IMU::ImuAlarmConfig cfg = config();
    cfg.baselineValid = false;
    IMU::ImuMetrics m = metrics(NAN);
    m.baselineValid = false;

    const IMU::ImuAlarmStatus status = detector.update(cfg, m, 1000);

    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_TRUE(status.decisionValid);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::NoBaseline, status.reason);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, status.triggerValue);
}

void test_tilt_requires_hold_before_triggering() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();

    IMU::ImuAlarmStatus status = detector.update(cfg, metrics(35.0f), 1000);
    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingTrigger);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Tilt, status.reason);

    status = detector.update(cfg, metrics(35.0f), 1500);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_FALSE(status.pendingTrigger);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Tilt, status.reason);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, status.triggerValue);
}

void test_tilt_hysteresis_and_clear_hold_before_clearing() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();

    detector.update(cfg, metrics(35.0f), 1000);
    IMU::ImuAlarmStatus status = detector.update(cfg, metrics(35.0f), 1500);
    TEST_ASSERT_TRUE(status.triggered);

    status = detector.update(cfg, metrics(26.0f), 2000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_FALSE(status.pendingClear);

    status = detector.update(cfg, metrics(24.0f), 2100);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);

    status = detector.update(cfg, metrics(24.0f), 3100);
    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::None, status.reason);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, status.triggerValue);
}

void test_stale_sample_is_unknown_and_restarts_fresh_clear_hold() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();

    detector.update(cfg, metrics(35.0f), 1000);
    IMU::ImuAlarmStatus status = detector.update(cfg, metrics(35.0f), 1500);
    TEST_ASSERT_TRUE(status.triggered);

    IMU::ImuMetrics stale = metrics(35.0f);
    stale.sampleFresh = false;
    stale.sampleTimestampKnown = true;
    status = detector.update(cfg, stale, 1600);

    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_FALSE(status.decisionValid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, status.triggerValue);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Stale, status.reason);

    // Wall time spent stale cannot complete the clear proof. The first fresh
    // quiet sample starts a new clear hold from its own timestamp.
    status = detector.update(cfg, metrics(0.0f), 5000);
    TEST_ASSERT_TRUE(status.decisionValid);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);
    TEST_ASSERT_EQUAL_UINT32(0, status.clearHoldElapsedMs);

    status = detector.update(cfg, metrics(0.0f), 5999);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);

    status = detector.update(cfg, metrics(0.0f), 6000);
    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_FALSE(status.pendingClear);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, status.triggerValue);
}

void test_unavailable_sample_preserves_clear_state_as_unknown() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();
    IMU::ImuMetrics unavailable{};

    const IMU::ImuAlarmStatus status = detector.update(cfg, unavailable, 1000);

    TEST_ASSERT_FALSE(status.sampleFresh);
    TEST_ASSERT_FALSE(status.decisionValid);
    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Unavailable, status.reason);
}

void test_disabled_alarm_produces_explicit_false_decision() {
    IMU::ImuAlarmDetector detector;
    IMU::ImuAlarmConfig cfg = config();
    detector.update(cfg, metrics(35.0f), 1000);
    TEST_ASSERT_TRUE(detector.update(cfg, metrics(35.0f), 1500).triggered);

    cfg.enabled = false;
    const IMU::ImuAlarmStatus status = detector.update(cfg, IMU::ImuMetrics{}, 1600);

    TEST_ASSERT_FALSE(status.enabled);
    TEST_ASSERT_TRUE(status.decisionValid);
    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, status.triggerValue);
}

void test_restored_trigger_requires_full_fresh_clear_hold() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();
    detector.restoreRetainedTrigger(true);

    IMU::ImuAlarmStatus status = detector.update(cfg, metrics(0.0f), 1000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);
    TEST_ASSERT_EQUAL_UINT32(0, status.clearHoldElapsedMs);

    status = detector.update(cfg, metrics(0.0f), 1999);
    TEST_ASSERT_TRUE(status.triggered);

    status = detector.update(cfg, metrics(0.0f), 2000);
    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_TRUE(status.decisionValid);
}

void test_active_tilt_missing_baseline_is_unknown_and_restarts_clear_hold() {
    IMU::ImuAlarmDetector detector;
    IMU::ImuAlarmConfig cfg = config();
    detector.update(cfg, metrics(35.0f), 1000);
    TEST_ASSERT_TRUE(detector.update(cfg, metrics(35.0f), 1500).triggered);

    IMU::ImuMetrics quiet = metrics(0.0f);
    IMU::ImuAlarmStatus status = detector.update(cfg, quiet, 1600);
    TEST_ASSERT_TRUE(status.pendingClear);

    cfg.baselineValid = false;
    quiet.baselineValid = false;
    quiet.tiltDeg = NAN;
    status = detector.update(cfg, quiet, 2500);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_FALSE(status.decisionValid);
    TEST_ASSERT_FALSE(status.pendingClear);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::NoBaseline, status.reason);

    cfg = config();
    quiet = metrics(0.0f);
    status = detector.update(cfg, quiet, 5000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);
    TEST_ASSERT_EQUAL_UINT32(0, status.clearHoldElapsedMs);

    status = detector.update(cfg, quiet, 6000);
    TEST_ASSERT_FALSE(status.triggered);
}

void test_active_tilt_nan_tilt_is_unknown_until_new_full_clear_hold() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();
    detector.update(cfg, metrics(35.0f), 1000);
    TEST_ASSERT_TRUE(detector.update(cfg, metrics(35.0f), 1500).triggered);

    IMU::ImuMetrics invalidTilt = metrics(NAN);
    IMU::ImuAlarmStatus status = detector.update(cfg, invalidTilt, 5000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_FALSE(status.decisionValid);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Unavailable, status.reason);

    status = detector.update(cfg, metrics(0.0f), 6000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);
    TEST_ASSERT_EQUAL_UINT32(0, status.clearHoldElapsedMs);

    status = detector.update(cfg, metrics(0.0f), 7000);
    TEST_ASSERT_FALSE(status.triggered);
}

void test_active_shock_can_clear_from_complete_fresh_acceleration_without_baseline() {
    IMU::ImuAlarmDetector detector;
    IMU::ImuAlarmConfig cfg = config();
    cfg.baselineValid = false;
    IMU::ImuMetrics shock = metrics(NAN, 0.5f);
    shock.baselineValid = false;
    TEST_ASSERT_TRUE(detector.update(cfg, shock, 1000).triggered);

    IMU::ImuMetrics quiet = metrics(NAN, 0.0f);
    quiet.baselineValid = false;
    IMU::ImuAlarmStatus status = detector.update(cfg, quiet, 2000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.decisionValid);
    TEST_ASSERT_TRUE(status.pendingClear);

    status = detector.update(cfg, quiet, 3000);
    TEST_ASSERT_FALSE(status.triggered);
}

void test_active_shock_with_invalid_acceleration_is_unknown() {
    IMU::ImuAlarmDetector detector;
    IMU::ImuAlarmConfig cfg = config();
    cfg.baselineValid = false;
    IMU::ImuMetrics shock = metrics(NAN, 0.5f);
    shock.baselineValid = false;
    TEST_ASSERT_TRUE(detector.update(cfg, shock, 1000).triggered);

    IMU::ImuMetrics invalid = metrics(NAN, NAN);
    invalid.baselineValid = false;
    IMU::ImuAlarmStatus status = detector.update(cfg, invalid, 5000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_FALSE(status.decisionValid);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Unavailable, status.reason);

    IMU::ImuMetrics quiet = metrics(NAN, 0.0f);
    quiet.baselineValid = false;
    status = detector.update(cfg, quiet, 6000);
    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_TRUE(status.pendingClear);
    TEST_ASSERT_EQUAL_UINT32(0, status.clearHoldElapsedMs);
}

void test_shock_triggers_without_baseline() {
    IMU::ImuAlarmDetector detector;
    IMU::ImuAlarmConfig cfg = config();
    cfg.baselineValid = false;
    IMU::ImuMetrics m = metrics(NAN, 0.5f);
    m.baselineValid = false;

    const IMU::ImuAlarmStatus status = detector.update(cfg, m, 1000);

    TEST_ASSERT_TRUE(status.triggered);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Shock, status.reason);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, status.triggerValue);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_no_baseline_reports_not_ready_without_triggering);
    RUN_TEST(test_tilt_requires_hold_before_triggering);
    RUN_TEST(test_tilt_hysteresis_and_clear_hold_before_clearing);
    RUN_TEST(test_stale_sample_is_unknown_and_restarts_fresh_clear_hold);
    RUN_TEST(test_unavailable_sample_preserves_clear_state_as_unknown);
    RUN_TEST(test_disabled_alarm_produces_explicit_false_decision);
    RUN_TEST(test_restored_trigger_requires_full_fresh_clear_hold);
    RUN_TEST(test_active_tilt_missing_baseline_is_unknown_and_restarts_clear_hold);
    RUN_TEST(test_active_tilt_nan_tilt_is_unknown_until_new_full_clear_hold);
    RUN_TEST(test_active_shock_can_clear_from_complete_fresh_acceleration_without_baseline);
    RUN_TEST(test_active_shock_with_invalid_acceleration_is_unknown);
    RUN_TEST(test_shock_triggers_without_baseline);
    return UNITY_END();
}
