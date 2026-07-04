#include <unity.h>

#include <atomic>
#include <functional>
#include <type_traits>

#include "../../src/shelly/wiring/ShellyCommandBridge.h"

using ExpectedBuildSignature = std::function<bool(uint8_t, bool)> (*)(
    const std::atomic<bool>*,
    std::function<SHELLY::ShellyService*()>
);

static_assert(
    std::is_same<decltype(&SHELLY::ShellyCommandBridge::build), ExpectedBuildSignature>::value,
    "ShellyCommandBridge::build must use lazy ShellyService provider to prevent stale nullptr capture"
);

void setUp() {}
void tearDown() {}

void test_shelly_bridge_contract(void) {
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_shelly_bridge_contract);
    return UNITY_END();
}
