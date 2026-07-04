/**
 * @file test_log_ring_buffer.cpp
 * @brief Unit tests for LogRingBuffer
 * 
 * Tests cover:
 * - Basic append and retrieve
 * - Ring buffer wrap-around behavior
 * - Capacity limits
 * - Message truncation
 * - Clear operation
 * - Null/empty input handling
 */

#include <unity.h>
#include <cstdio>
#include <cstring>
#include <array>
#include <vector>

#define NATIVE_BUILD 1

// Mock FreeRTOS
typedef void* SemaphoreHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)

static bool g_mutexTaken = false;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { 
    static int dummy = 0;
    return &dummy; 
}

inline int xSemaphoreTake(SemaphoreHandle_t mutex, uint32_t timeout) {
    (void)mutex; (void)timeout;
    g_mutexTaken = true;
    return pdTRUE;
}

inline int xSemaphoreGive(SemaphoreHandle_t mutex) {
    (void)mutex;
    g_mutexTaken = false;
    return pdTRUE;
}

// Mock Arduino
inline uint32_t millis() { 
    static uint32_t ms = 0;
    return ms += 100; // Increment each call
}

// Mock ESP reset reason
typedef enum { ESP_RST_UNKNOWN, ESP_RST_DEEPSLEEP } esp_reset_reason_t;
inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_UNKNOWN; }

// RTC attribute mock (just regular memory in native)
#define RTC_DATA_ATTR

// Include the header
namespace LOG {

struct Line {
    uint32_t timestampMs{0};
    char levelChar{'N'};
    char tag[12]{};
    char message[128]{};
};

class RingBuffer {
public:
    static constexpr size_t kFixedCapacity = 20;
    
    static void begin(uint16_t capacity);
    static void append(const char *levelLabel, const char *tag, const char *message);
    
    template <typename Callback>
    static void forEachTail(size_t maxLines, Callback&& callback) {
        if (!_mutex || maxLines == 0) return;
        
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
        
        size_t toRead = (_count < maxLines) ? _count : maxLines;
        if (toRead == 0) {
            xSemaphoreGive(_mutex);
            return;
        }
        
        std::array<Line, kFixedCapacity> snapshot{};
        size_t start = (_head + _buffer.size() - toRead) % _buffer.size();
        for (size_t i = 0; i < toRead; i++) {
            size_t idx = (start + i) % _buffer.size();
            snapshot[i] = _buffer[idx];
        }
        
        xSemaphoreGive(_mutex);
        
        for (size_t i = 0; i < toRead; i++) {
            callback(snapshot[i]);
        }
    }
    
    static void clear();
    static void resize(uint16_t newSize);
    static size_t count() { return _count; }
    static size_t capacity() { return kFixedCapacity; }

private:
    static std::array<Line, kFixedCapacity> _buffer;
    static size_t _head;
    static size_t _count;
    static uint32_t _magic;
    static SemaphoreHandle_t _mutex;
};

// Static member definitions
std::array<Line, RingBuffer::kFixedCapacity> RingBuffer::_buffer;
size_t RingBuffer::_head = 0;
size_t RingBuffer::_count = 0;
uint32_t RingBuffer::_magic = 0;
SemaphoreHandle_t RingBuffer::_mutex = nullptr;

void RingBuffer::begin(uint16_t capacity) {
    (void)capacity;
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }
    _head = 0;
    _count = 0;
    _buffer.fill(Line{});
    _magic = 0x4C4F4742;
}

void RingBuffer::append(const char *levelLabel, const char *tag, const char *message) {
    if (!_mutex) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    
    Line line{};
    line.timestampMs = millis();
    line.levelChar = (levelLabel && levelLabel[0]) ? levelLabel[0] : 'N';
    
    if (tag) {
        strncpy(line.tag, tag, sizeof(line.tag) - 1);
        line.tag[sizeof(line.tag) - 1] = '\0';
    }
    
    if (message) {
        strncpy(line.message, message, sizeof(line.message) - 1);
        line.message[sizeof(line.message) - 1] = '\0';
    }
    
    _buffer[_head] = line;
    _head = (_head + 1) % _buffer.size();
    if (_count < _buffer.size()) {
        _count++;
    }
    
    xSemaphoreGive(_mutex);
}

void RingBuffer::clear() {
    if (!_mutex) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        _head = 0;
        _count = 0;
        _buffer.fill(Line{});
        xSemaphoreGive(_mutex);
    }
}

void RingBuffer::resize(uint16_t newSize) {
    (void)newSize;
}

}  // namespace LOG

using namespace LOG;

void setUp(void) {
    RingBuffer::begin(20);
}

void tearDown(void) {}

// ============================================================================
// Test: Basic operations
// ============================================================================

void test_empty_buffer() {
    TEST_ASSERT_EQUAL(0, RingBuffer::count());
    TEST_ASSERT_EQUAL(20, RingBuffer::capacity());
}

void test_append_single() {
    RingBuffer::append("I", "Test", "Hello World");
    TEST_ASSERT_EQUAL(1, RingBuffer::count());
}

void test_append_and_retrieve() {
    RingBuffer::append("I", "Sensor", "Temperature: 25.0C");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(10, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL('I', lines[0].levelChar);
    TEST_ASSERT_EQUAL_STRING("Sensor", lines[0].tag);
    TEST_ASSERT_EQUAL_STRING("Temperature: 25.0C", lines[0].message);
}

void test_level_char_extraction() {
    RingBuffer::append("E", "Error", "Something bad");
    RingBuffer::append("W", "Warn", "Something suspicious");
    RingBuffer::append("D", "Debug", "Verbose info");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(10, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(3, lines.size());
    TEST_ASSERT_EQUAL('E', lines[0].levelChar);
    TEST_ASSERT_EQUAL('W', lines[1].levelChar);
    TEST_ASSERT_EQUAL('D', lines[2].levelChar);
}

// ============================================================================
// Test: Ring buffer wrap-around
// ============================================================================

void test_wrap_around() {
    // Fill buffer completely
    for (int i = 0; i < 20; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);
        RingBuffer::append("I", "Test", msg);
    }
    
    TEST_ASSERT_EQUAL(20, RingBuffer::count());
    
    // Add one more - should wrap
    RingBuffer::append("I", "Test", "Message 20");
    
    // Still 20 (capacity limit)
    TEST_ASSERT_EQUAL(20, RingBuffer::count());
    
    // Verify oldest is gone, newest is present
    std::vector<Line> lines;
    RingBuffer::forEachTail(20, [&](const Line& line) {
        lines.push_back(line);
    });
    
    // First should be "Message 1" (Message 0 was overwritten)
    TEST_ASSERT_EQUAL_STRING("Message 1", lines[0].message);
    // Last should be "Message 20"
    TEST_ASSERT_EQUAL_STRING("Message 20", lines[19].message);
}

void test_partial_tail_retrieval() {
    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Log %d", i);
        RingBuffer::append("I", "Tag", msg);
    }
    
    // Request only last 3
    std::vector<Line> lines;
    RingBuffer::forEachTail(3, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(3, lines.size());
    TEST_ASSERT_EQUAL_STRING("Log 7", lines[0].message);
    TEST_ASSERT_EQUAL_STRING("Log 8", lines[1].message);
    TEST_ASSERT_EQUAL_STRING("Log 9", lines[2].message);
}

// ============================================================================
// Test: Truncation
// ============================================================================

void test_tag_truncation() {
    // Tag buffer is 12 chars
    RingBuffer::append("I", "VeryLongTagNameThatExceedsBuffer", "msg");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(1, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(11, strlen(lines[0].tag)); // 11 chars + null
    TEST_ASSERT_EQUAL_STRING("VeryLongTag", lines[0].tag);
}

void test_message_truncation() {
    // Message buffer is 128 chars
    char longMsg[256];
    memset(longMsg, 'X', 255);
    longMsg[255] = '\0';
    
    RingBuffer::append("I", "Tag", longMsg);
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(1, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(127, strlen(lines[0].message)); // 127 chars + null
}

// ============================================================================
// Test: Clear
// ============================================================================

void test_clear() {
    RingBuffer::append("I", "Tag", "Message 1");
    RingBuffer::append("I", "Tag", "Message 2");
    TEST_ASSERT_EQUAL(2, RingBuffer::count());
    
    RingBuffer::clear();
    TEST_ASSERT_EQUAL(0, RingBuffer::count());
}

// ============================================================================
// Test: Null/empty handling
// ============================================================================

void test_null_tag() {
    RingBuffer::append("I", nullptr, "Message");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(1, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL('\0', lines[0].tag[0]);
}

void test_null_message() {
    RingBuffer::append("I", "Tag", nullptr);
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(1, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL('\0', lines[0].message[0]);
}

void test_null_level() {
    RingBuffer::append(nullptr, "Tag", "Message");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(1, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(1, lines.size());
    TEST_ASSERT_EQUAL('N', lines[0].levelChar); // Default
}

void test_empty_level() {
    RingBuffer::append("", "Tag", "Message");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(1, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL('N', lines[0].levelChar);
}

// ============================================================================
// Test: Edge cases
// ============================================================================

void test_request_more_than_available() {
    RingBuffer::append("I", "Tag", "Only one");
    
    std::vector<Line> lines;
    RingBuffer::forEachTail(100, [&](const Line& line) {
        lines.push_back(line);
    });
    
    TEST_ASSERT_EQUAL(1, lines.size());
}

void test_request_zero_lines() {
    RingBuffer::append("I", "Tag", "Message");
    
    int callCount = 0;
    RingBuffer::forEachTail(0, [&](const Line& line) {
        (void)line;
        callCount++;
    });
    
    TEST_ASSERT_EQUAL(0, callCount);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Basic ops
    RUN_TEST(test_empty_buffer);
    RUN_TEST(test_append_single);
    RUN_TEST(test_append_and_retrieve);
    RUN_TEST(test_level_char_extraction);
    
    // Wrap-around
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_partial_tail_retrieval);
    
    // Truncation
    RUN_TEST(test_tag_truncation);
    RUN_TEST(test_message_truncation);
    
    // Clear
    RUN_TEST(test_clear);
    
    // Null/empty
    RUN_TEST(test_null_tag);
    RUN_TEST(test_null_message);
    RUN_TEST(test_null_level);
    RUN_TEST(test_empty_level);
    
    // Edge cases
    RUN_TEST(test_request_more_than_available);
    RUN_TEST(test_request_zero_lines);
    
    return UNITY_END();
}
