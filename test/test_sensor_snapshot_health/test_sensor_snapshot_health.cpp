#include <unity.h>

#include "../../src/sensors/runtime/SensorSnapshotHealth.h"

using SENSORS::isSnapshotFresh;
using SENSORS::shouldPromoteNoDataToStale;

void test_marks_zero_timestamp_as_not_fresh() {
    TEST_ASSERT_FALSE(isSnapshotFresh(0, 1000, 5000));
}

void test_treats_recent_snapshot_as_fresh() {
    TEST_ASSERT_TRUE(isSnapshotFresh(5000, 9000, 5000));
}

void test_treats_timeout_boundary_as_stale() {
    TEST_ASSERT_FALSE(isSnapshotFresh(5000, 10000, 5000));
}

void test_promotes_no_data_to_stale_once_timeout_is_reached() {
    TEST_ASSERT_TRUE(shouldPromoteNoDataToStale(5000, 10000, 5000, false));
}

void test_does_not_promote_when_already_stale_or_without_snapshot() {
    TEST_ASSERT_FALSE(shouldPromoteNoDataToStale(5000, 12000, 5000, true));
    TEST_ASSERT_FALSE(shouldPromoteNoDataToStale(0, 12000, 5000, false));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_marks_zero_timestamp_as_not_fresh);
    RUN_TEST(test_treats_recent_snapshot_as_fresh);
    RUN_TEST(test_treats_timeout_boundary_as_stale);
    RUN_TEST(test_promotes_no_data_to_stale_once_timeout_is_reached);
    RUN_TEST(test_does_not_promote_when_already_stale_or_without_snapshot);
    return UNITY_END();
}
