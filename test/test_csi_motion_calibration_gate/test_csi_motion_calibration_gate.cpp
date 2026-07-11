#include <unity.h>

#include "../../src/wifisensing/csi/core/CsiMotionCalibrationGate.h"

using WIFISENSING::CSI::CsiMotionCalibrationGate;
using WIFISENSING::CSI::CsiMotionSnapshot;

namespace {

constexpr uint32_t kStaleAfterMs = 5000;

bool canStart(const CsiMotionSnapshot& snapshot,
              uint32_t nowMs,
              bool configEnabled = true,
              uint8_t bandCount = 1,
              bool runtimeEnabled = true,
              bool storageReady = true,
              bool gainForced = true,
              bool runtimeFault = false) {
    return CsiMotionCalibrationGate::canStart(
        configEnabled,
        bandCount,
        runtimeEnabled,
        storageReady,
        gainForced,
        runtimeFault,
        snapshot,
        nowMs,
        kStaleAfterMs);
}

} // namespace

void setUp() {}
void tearDown() {}

void test_accepts_live_pipeline_with_fresh_frame() {
    CsiMotionSnapshot snapshot;
    snapshot.hasFrame = true;
    snapshot.lastFrameMs = 1000;

    TEST_ASSERT_TRUE(canStart(snapshot, 5999));
    TEST_ASSERT_TRUE(canStart(snapshot, 6000));
}

void test_rejects_missing_or_stale_frame() {
    CsiMotionSnapshot snapshot;
    TEST_ASSERT_FALSE(canStart(snapshot, 1000));

    snapshot.hasFrame = true;
    snapshot.lastFrameMs = 1000;
    TEST_ASSERT_FALSE(canStart(snapshot, 6001));
}

void test_rejects_unavailable_snapshot_even_with_recent_previous_frame() {
    CsiMotionSnapshot snapshot;
    snapshot.state = WIFISENSING::CSI::CsiMotionState::Unavailable;
    snapshot.hasFrame = true;
    snapshot.lastFrameMs = 1000;

    TEST_ASSERT_FALSE(canStart(snapshot, 1001));
}

void test_rejects_incomplete_or_faulted_pipeline() {
    CsiMotionSnapshot snapshot;
    snapshot.hasFrame = true;
    snapshot.lastFrameMs = 1000;

    TEST_ASSERT_FALSE(canStart(snapshot, 1000, false));
    TEST_ASSERT_FALSE(canStart(snapshot, 1000, true, 0));
    TEST_ASSERT_FALSE(canStart(snapshot, 1000, true, 1, false));
    TEST_ASSERT_FALSE(canStart(snapshot, 1000, true, 1, true, false));
    TEST_ASSERT_FALSE(canStart(snapshot, 1000, true, 1, true, true, false));
    TEST_ASSERT_FALSE(canStart(snapshot, 1000, true, 1, true, true, true, true));
}

void test_frame_age_is_wrap_safe() {
    CsiMotionSnapshot snapshot;
    snapshot.hasFrame = true;
    snapshot.lastFrameMs = 0xfffffff0u;

    TEST_ASSERT_TRUE(canStart(snapshot, 0x00000010u));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_accepts_live_pipeline_with_fresh_frame);
    RUN_TEST(test_rejects_missing_or_stale_frame);
    RUN_TEST(test_rejects_unavailable_snapshot_even_with_recent_previous_frame);
    RUN_TEST(test_rejects_incomplete_or_faulted_pipeline);
    RUN_TEST(test_frame_age_is_wrap_safe);
    return UNITY_END();
}
