#include <unity.h>

#include "../../src/wifisensing/csi/core/CsiMotionBootstrapGate.h"

void setUp(void) {}
void tearDown(void) {}

void test_bootstrap_suppresses_constructor_default_until_config_completes() {
    WIFISENSING::CSI::CsiMotionBootstrapGate gate;

    TEST_ASSERT_FALSE(gate.pending());
    gate.begin(true);
    TEST_ASSERT_TRUE(gate.pending());
    TEST_ASSERT_TRUE(gate.retained());
    gate.complete();
    TEST_ASSERT_FALSE(gate.pending());
    TEST_ASSERT_TRUE(gate.retained());
}

void test_repeated_bootstrap_begin_remains_pending_until_terminal_apply() {
    WIFISENSING::CSI::CsiMotionBootstrapGate gate;

    gate.begin(false);
    gate.begin(true);
    TEST_ASSERT_TRUE(gate.pending());
    TEST_ASSERT_TRUE(gate.retained());
    gate.complete();
    TEST_ASSERT_FALSE(gate.pending());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_bootstrap_suppresses_constructor_default_until_config_completes);
    RUN_TEST(test_repeated_bootstrap_begin_remains_pending_until_terminal_apply);
    return UNITY_END();
}
