/**
 * @file test_ble_config_logic.cpp
 * @brief Unit tests for BleConfig logic (Value Object verification)
 */

#include <unity.h>
#include "../../test/stubs/mocks/BleTypes.h"

using namespace BLE;

void setUp(void) {}
void tearDown(void) {}

void test_ble_config_defaults_are_safe() {
    BleConfig config;
    
    TEST_ASSERT_FALSE(config.enabled);
}

void test_ble_config_copy_semantics() {
    BleConfig original;
    original.enabled = true;
    
    BleConfig copy = original;
    
    TEST_ASSERT_TRUE(copy.enabled);
    
    copy.enabled = false;
    TEST_ASSERT_TRUE(original.enabled);
    TEST_ASSERT_FALSE(copy.enabled);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_ble_config_defaults_are_safe);
    RUN_TEST(test_ble_config_copy_semantics);
    
    return UNITY_END();
}
