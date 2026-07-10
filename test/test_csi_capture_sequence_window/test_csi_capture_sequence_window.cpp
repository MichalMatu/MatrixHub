#include <unity.h>

#include "../../src/api/wifisensing/CsiCaptureSequenceWindow.h"

namespace {

struct Packet {
    uint32_t acceptedSequence = 0;
};

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_start_fence_excludes_backlog() {
    const Packet packets[] = {{99}, {100}, {101}, {102}};
    const auto slice = API::CSI_CAPTURE::selectBatchSlice(
        packets, 4, 100, false, 100);

    TEST_ASSERT_EQUAL_UINT32(2, slice.offset);
    TEST_ASSERT_EQUAL_UINT32(2, slice.count);
    TEST_ASSERT_FALSE(slice.reachedStopBoundary);
}

void test_stop_fence_includes_boundary_and_excludes_tail() {
    const Packet packets[] = {{101}, {102}, {103}, {104}};
    const auto slice = API::CSI_CAPTURE::selectBatchSlice(
        packets, 4, 100, true, 103);

    TEST_ASSERT_EQUAL_UINT32(0, slice.offset);
    TEST_ASSERT_EQUAL_UINT32(3, slice.count);
    TEST_ASSERT_TRUE(slice.reachedStopBoundary);
}

void test_stop_fence_finishes_after_missing_boundary() {
    const Packet packets[] = {{104}, {105}};
    const auto slice = API::CSI_CAPTURE::selectBatchSlice(
        packets, 2, 100, true, 103);

    TEST_ASSERT_EQUAL_UINT32(0, slice.count);
    TEST_ASSERT_TRUE(slice.reachedStopBoundary);
}

void test_sequence_wrap_is_ordered_modularly() {
    const Packet packets[] = {{0xffffffffu}, {0}, {1}, {2}};
    const auto slice = API::CSI_CAPTURE::selectBatchSlice(
        packets, 4, 0xffffffffu, true, 1);

    TEST_ASSERT_EQUAL_UINT32(1, slice.offset);
    TEST_ASSERT_EQUAL_UINT32(2, slice.count);
    TEST_ASSERT_TRUE(slice.reachedStopBoundary);
}

void test_empty_stop_window_finishes_without_records() {
    const Packet packets[] = {{11}};
    const auto slice = API::CSI_CAPTURE::selectBatchSlice(
        packets, 1, 10, true, 10);

    TEST_ASSERT_EQUAL_UINT32(0, slice.count);
    TEST_ASSERT_TRUE(slice.reachedStopBoundary);
}

void test_complete_window_rejects_missing_first_packet() {
    TEST_ASSERT_FALSE(API::CSI_CAPTURE::isCompleteWindow(100, 103, 2, 102, 103));
    TEST_ASSERT_TRUE(API::CSI_CAPTURE::isCompleteWindow(100, 103, 3, 101, 103));
}

void test_complete_window_rejects_missing_boundary_packet() {
    TEST_ASSERT_FALSE(API::CSI_CAPTURE::isCompleteWindow(100, 103, 2, 101, 102));
    TEST_ASSERT_TRUE(API::CSI_CAPTURE::isCompleteWindow(100, 100, 0, 0, 0));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_start_fence_excludes_backlog);
    RUN_TEST(test_stop_fence_includes_boundary_and_excludes_tail);
    RUN_TEST(test_stop_fence_finishes_after_missing_boundary);
    RUN_TEST(test_sequence_wrap_is_ordered_modularly);
    RUN_TEST(test_empty_stop_window_finishes_without_records);
    RUN_TEST(test_complete_window_rejects_missing_first_packet);
    RUN_TEST(test_complete_window_rejects_missing_boundary_packet);
    return UNITY_END();
}
