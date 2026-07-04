#include <unity.h>

#include "../../src/api/filemanager/handlers/FileManagerLogProtection.h"

using API::FILEMANAGER::isLogStoragePath;
using API::FILEMANAGER::isSamePathOrParentDirectory;
using API::FILEMANAGER::shouldAcquireLogReadLease;
using API::FILEMANAGER::shouldProtectActiveLogPath;

void test_detects_data_path_as_log_storage() {
    TEST_ASSERT_TRUE(isLogStoragePath("/data/2026-04/2026-04-01.bin"));
    TEST_ASSERT_FALSE(isLogStoragePath("/config/config.json"));
    TEST_ASSERT_FALSE(isLogStoragePath("/data"));
}

void test_protects_active_log_and_parent_directories() {
    const char* active = "/data/2026-04/2026-04-01.bin";
    TEST_ASSERT_TRUE(shouldProtectActiveLogPath("/data/2026-04/2026-04-01.bin", active));
    TEST_ASSERT_TRUE(shouldProtectActiveLogPath("/data/2026-04", active));
    TEST_ASSERT_TRUE(shouldProtectActiveLogPath("/data", active));
    TEST_ASSERT_FALSE(shouldProtectActiveLogPath("/data/2026-03", active));
    TEST_ASSERT_FALSE(shouldProtectActiveLogPath("/config", active));
}

void test_parent_match_does_not_accept_prefix_only_siblings() {
    TEST_ASSERT_FALSE(isSamePathOrParentDirectory("/data/2026-0", "/data/2026-04/file.bin"));
    TEST_ASSERT_FALSE(isSamePathOrParentDirectory("/data/2026-04x", "/data/2026-04/file.bin"));
}

void test_acquires_log_read_lease_only_for_large_log_downloads() {
    TEST_ASSERT_TRUE(shouldAcquireLogReadLease("/data/2026-04/2026-04-01.bin", 5000, 1024));
    TEST_ASSERT_FALSE(shouldAcquireLogReadLease("/data/2026-04/2026-04-01.bin", 512, 1024));
    TEST_ASSERT_FALSE(shouldAcquireLogReadLease("/config/config.json", 5000, 1024));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_detects_data_path_as_log_storage);
    RUN_TEST(test_protects_active_log_and_parent_directories);
    RUN_TEST(test_parent_match_does_not_accept_prefix_only_siblings);
    RUN_TEST(test_acquires_log_read_lease_only_for_large_log_downloads);
    return UNITY_END();
}
