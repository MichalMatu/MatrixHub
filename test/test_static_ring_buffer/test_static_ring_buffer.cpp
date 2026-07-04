/**
 * @file test_static_ring_buffer.cpp
 * @brief Unit tests for StaticRingBuffer template
 */

#include <unity.h>
#include <string.h>
#include <vector>
#include "../../src/system/utils/StaticRingBuffer.h"

// Test Types
struct SimpleAcc {
    int sum = 0;
    void operator()(int val) { sum += val; }
};

struct VectorAcc {
    std::vector<int> items;
    void operator()(int val) { items.push_back(val); }
};

// ============================================================================
// Tests
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

void test_initial_state() {
    StaticRingBuffer<int, 5> buf;
    buf.clear();
    
    TEST_ASSERT_EQUAL(0, buf.size());
    TEST_ASSERT_EQUAL(5, buf.capacity());
}

void test_push_under_capacity() {
    StaticRingBuffer<int, 5> buf;
    buf.clear();
    
    buf.push(10);
    buf.push(20);
    buf.push(30);
    
    TEST_ASSERT_EQUAL(3, buf.size());
    
    VectorAcc acc;
    buf.forEach(std::ref(acc));
    
    TEST_ASSERT_EQUAL(3, acc.items.size());
    TEST_ASSERT_EQUAL(10, acc.items[0]);
    TEST_ASSERT_EQUAL(20, acc.items[1]);
    TEST_ASSERT_EQUAL(30, acc.items[2]);
}

void test_push_overflow() {
    StaticRingBuffer<int, 3> buf;
    buf.clear();
    
    buf.push(1);
    buf.push(2);
    buf.push(3);
    
    TEST_ASSERT_EQUAL(3, buf.size());
    
    // Overflow 1
    buf.push(4);
    // Should now contain [2, 3, 4]
    
    TEST_ASSERT_EQUAL(3, buf.size());
    
    VectorAcc acc;
    buf.forEach(std::ref(acc));
    
    TEST_ASSERT_EQUAL(3, acc.items.size());
    TEST_ASSERT_EQUAL(2, acc.items[0]);
    TEST_ASSERT_EQUAL(3, acc.items[1]);
    TEST_ASSERT_EQUAL(4, acc.items[2]);
}

void test_foreach_tail() {
    StaticRingBuffer<int, 10> buf;
    buf.clear();
    
    for (int i = 1; i <= 5; i++) buf.push(i);
    // [1, 2, 3, 4, 5]
    
    // Tail 3 -> [3, 4, 5]
    VectorAcc acc;
    buf.forEachTail(3, std::ref(acc));
    
    TEST_ASSERT_EQUAL(3, acc.items.size());
    TEST_ASSERT_EQUAL(3, acc.items[0]);
    TEST_ASSERT_EQUAL(5, acc.items[2]);
}

void test_foreach_tail_wrapped() {
    StaticRingBuffer<int, 3> buf;
    buf.clear();
    
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.push(4); // Overwrites 1 -> [2, 3, 4]
    
    // Tail 2 -> [3, 4]
    VectorAcc acc;
    buf.forEachTail(2, std::ref(acc));
    
    TEST_ASSERT_EQUAL(2, acc.items.size());
    TEST_ASSERT_EQUAL(3, acc.items[0]);
    TEST_ASSERT_EQUAL(4, acc.items[1]);
}

void test_foreach_tail_more_than_size() {
    StaticRingBuffer<int, 5> buf;
    buf.clear();
    
    buf.push(1);
    buf.push(2);
    
    // Request tail 5 on size 2 -> should get [1, 2]
    VectorAcc acc;
    buf.forEachTail(5, std::ref(acc));
    
    TEST_ASSERT_EQUAL(2, acc.items.size());
    TEST_ASSERT_EQUAL(1, acc.items[0]);
    TEST_ASSERT_EQUAL(2, acc.items[1]);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_push_under_capacity);
    RUN_TEST(test_push_overflow);
    RUN_TEST(test_foreach_tail);
    RUN_TEST(test_foreach_tail_wrapped);
    RUN_TEST(test_foreach_tail_more_than_size);
    return UNITY_END();
}
