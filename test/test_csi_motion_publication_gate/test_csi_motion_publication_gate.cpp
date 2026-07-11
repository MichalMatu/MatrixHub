#include <unity.h>

#include "../../src/wifisensing/csi/core/CsiMotionPublicationGate.h"

using WIFISENSING::CSI::CsiMotionPublicationGate;
using WIFISENSING::CSI::CsiMotionSnapshot;
using WIFISENSING::CSI::CsiMotionState;

namespace {

constexpr uint32_t kKeepaliveMs = 3000;

CsiMotionSnapshot snapshot(CsiMotionState state, bool motion) {
    CsiMotionSnapshot value;
    value.state = state;
    value.motion = motion;
    value.decisionValid = WIFISENSING::CSI::isCsiMotionDecisionValid(state);
    return value;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_first_definitive_false_is_published() {
    CsiMotionPublicationGate gate;
    const auto clear = snapshot(CsiMotionState::Monitoring, false);

    TEST_ASSERT_TRUE(gate.shouldPublish(clear, 1000, kKeepaliveMs));
    gate.markPublished(false, 1000);
    TEST_ASSERT_FALSE(gate.shouldPublish(clear, 2000, kKeepaliveMs));
}

void test_unknown_state_never_clears_a_published_motion() {
    CsiMotionPublicationGate gate;
    const auto motion = snapshot(CsiMotionState::MotionConfirmed, true);
    gate.markPublished(true, 1000);

    const auto noisy = snapshot(CsiMotionState::NoisyEnvironment, false);
    const auto unavailable = snapshot(CsiMotionState::Unavailable, false);
    TEST_ASSERT_FALSE(gate.shouldPublish(noisy, 2000, kKeepaliveMs));
    TEST_ASSERT_FALSE(gate.shouldPublish(unavailable, 3000, kKeepaliveMs));
    TEST_ASSERT_TRUE(gate.shouldPublish(motion, 4000, kKeepaliveMs));
}

void test_definitive_clear_is_published_after_unknown_period() {
    CsiMotionPublicationGate gate;
    gate.markPublished(true, 1000);

    const auto clear = snapshot(CsiMotionState::Monitoring, false);
    TEST_ASSERT_TRUE(gate.shouldPublish(clear, 5000, kKeepaliveMs));
    gate.markPublished(false, 5000);
    TEST_ASSERT_FALSE(gate.shouldPublish(clear, 6000, kKeepaliveMs));
}

void test_replacing_callback_invalidates_previous_delivery_state() {
    CsiMotionPublicationGate gate;
    const auto clear = snapshot(CsiMotionState::Monitoring, false);
    gate.markPublished(false, 1000);
    TEST_ASSERT_FALSE(gate.shouldPublish(clear, 1500, kKeepaliveMs));

    gate.invalidate();
    TEST_ASSERT_TRUE(gate.shouldPublish(clear, 1500, kKeepaliveMs));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_first_definitive_false_is_published);
    RUN_TEST(test_unknown_state_never_clears_a_published_motion);
    RUN_TEST(test_definitive_clear_is_published_after_unknown_period);
    RUN_TEST(test_replacing_callback_invalidates_previous_delivery_state);
    return UNITY_END();
}
