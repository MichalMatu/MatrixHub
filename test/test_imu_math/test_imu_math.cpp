#include <unity.h>

#include "../../src/sensors/imu/ImuMath.h"
#include "../../src/sensors/imu/ImuMath.cpp"

using IMU::ImuVector3;

void setUp(void) {}
void tearDown(void) {}

void test_tilt_degrees_zero_for_same_gravity_vector() {
    const ImuVector3 baseline{0.0f, 0.0f, 1.0f};
    const ImuVector3 current{0.0f, 0.0f, 1.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, IMU::MATH::tiltDegrees(baseline, current));
}

void test_tilt_degrees_handles_right_angle() {
    const ImuVector3 baseline{0.0f, 0.0f, 1.0f};
    const ImuVector3 current{1.0f, 0.0f, 0.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, IMU::MATH::tiltDegrees(baseline, current));
}

void test_tilt_degrees_normalizes_non_unit_vectors() {
    const ImuVector3 baseline{0.0f, 0.0f, 2.0f};
    const ImuVector3 current{0.0f, 1.0f, 1.0f};

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 45.0f, IMU::MATH::tiltDegrees(baseline, current));
}

void test_normalize_rejects_invalid_baseline_vector() {
    ImuVector3 normalized;

    TEST_ASSERT_FALSE(IMU::MATH::normalize({0.0f, 0.0f, 0.0f}, normalized));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, normalized.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, normalized.y);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, normalized.z);
}

void test_accel_stability_window_matches_orientation_calibration_guard() {
    TEST_ASSERT_TRUE(IMU::MATH::isAccelMagnitudeStable(1.0f));
    TEST_ASSERT_TRUE(IMU::MATH::isAccelMagnitudeStable(0.85f));
    TEST_ASSERT_TRUE(IMU::MATH::isAccelMagnitudeStable(1.15f));
    TEST_ASSERT_FALSE(IMU::MATH::isAccelMagnitudeStable(0.84f));
    TEST_ASSERT_FALSE(IMU::MATH::isAccelMagnitudeStable(1.16f));
}

void test_elapsed_ms_handles_normal_and_wrapped_timestamps() {
    TEST_ASSERT_EQUAL_UINT32(250u, IMU::MATH::elapsedMs(1250u, 1000u));
    TEST_ASSERT_EQUAL_UINT32(32u, IMU::MATH::elapsedMs(15u, 0xFFFFFFEFu));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_tilt_degrees_zero_for_same_gravity_vector);
    RUN_TEST(test_tilt_degrees_handles_right_angle);
    RUN_TEST(test_tilt_degrees_normalizes_non_unit_vectors);
    RUN_TEST(test_normalize_rejects_invalid_baseline_vector);
    RUN_TEST(test_accel_stability_window_matches_orientation_calibration_guard);
    RUN_TEST(test_elapsed_ms_handles_normal_and_wrapped_timestamps);
    return UNITY_END();
}
