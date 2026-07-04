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

void test_stale_sample_clears_and_reports_stale() {
    IMU::ImuAlarmDetector detector;
    const IMU::ImuAlarmConfig cfg = config();

    detector.update(cfg, metrics(35.0f), 1000);
    IMU::ImuAlarmStatus status = detector.update(cfg, metrics(35.0f), 1500);
    TEST_ASSERT_TRUE(status.triggered);

    IMU::ImuMetrics stale = metrics(35.0f);
    stale.sampleFresh = false;
    stale.sampleTimestampKnown = true;
    status = detector.update(cfg, stale, 1600);

    TEST_ASSERT_FALSE(status.triggered);
    TEST_ASSERT_EQUAL(IMU::ImuAlarmReason::Stale, status.reason);
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
    RUN_TEST(test_stale_sample_clears_and_reports_stale);
    RUN_TEST(test_shock_triggers_without_baseline);
    return UNITY_END();
}
