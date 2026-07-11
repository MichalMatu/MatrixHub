#include <unity.h>

#define MATRIXHUB_GIT_SHA "0123456789abcdef0123456789abcdef01234567"
#define MATRIXHUB_GIT_DIRTY 0
#include "../../src/system/build/FirmwareIdentity.h"

void setUp(void) {}
void tearDown(void) {}

void test_compile_time_firmware_identity_preserves_sha_and_clean_bit() {
    TEST_ASSERT_EQUAL_STRING(
        "0123456789abcdef0123456789abcdef01234567",
        SYSTEM::BUILD::FIRMWARE_COMMIT);
    TEST_ASSERT_FALSE(SYSTEM::BUILD::FIRMWARE_DIRTY);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_compile_time_firmware_identity_preserves_sha_and_clean_bit);
    return UNITY_END();
}
