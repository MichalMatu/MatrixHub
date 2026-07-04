#include <unity.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/system/health/network/HttpServerHealthTracker.h"
#include "../../src/system/health/network/HttpServerHealthTracker.cpp"

namespace {

struct CapturedLog {
    esp_log_level_t level;
    std::string tag;
    std::string message;
};

std::vector<CapturedLog> s_logs;

}  // namespace

namespace LOG {
Settings Logging::_settings{ESP_LOG_INFO};

void Logging::setLevel(esp_log_level_t level) {
    _settings.level = level;
}

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    if (level > _settings.level) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    s_logs.push_back({
        level,
        tag ? tag : "",
        buffer,
    });
}

void Logging::logSection(const char* title) { (void)title; }

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}

esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
}  // namespace LOG

using namespace SYSTEM::HEALTH;

void setUp(void) {
    HttpServerHealthTracker::reset();
    s_logs.clear();
    LOG::Logging::setLevel(ESP_LOG_INFO);
}

void tearDown(void) {}

void test_counts_open_close_and_peak_clients() {
    HttpServerHealthTracker::recordOpen();
    HttpServerHealthTracker::recordOpen();
    HttpServerHealthTracker::recordClose();

    const auto snapshot = HttpServerHealthTracker::getSnapshot();
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.activeClients);
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.peakClients);
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.openCount);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.closeCount);
}

void test_counts_forced_ws_removals() {
    HttpServerHealthTracker::recordOpen();
    HttpServerHealthTracker::recordWsForcedRemoval(42);
    HttpServerHealthTracker::recordClose();

    const auto snapshot = HttpServerHealthTracker::getSnapshot();
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.wsForcedRemovals);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.openCount);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.closeCount);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.activeClients);
}

void test_counts_websocket_session_lifecycle_separately_from_http_clients() {
    TEST_STUBS::ARDUINO::millisValue = 100;
    HttpServerHealthTracker::recordOpen();
    HttpServerHealthTracker::recordWsOpen();
    HttpServerHealthTracker::recordWsOpen();

    TEST_STUBS::ARDUINO::millisValue = 250;
    HttpServerHealthTracker::recordWsClose();
    HttpServerHealthTracker::recordWsClose();
    HttpServerHealthTracker::recordWsClose();

    const auto snapshot = HttpServerHealthTracker::getSnapshot();
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.activeClients);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.openCount);
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.wsOpenCount);
    TEST_ASSERT_EQUAL_UINT32(3, snapshot.wsCloseCount);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.wsActiveClients);
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.wsPeakClients);
    TEST_ASSERT_EQUAL_UINT32(100, snapshot.lastWsOpenMs);
    TEST_ASSERT_EQUAL_UINT32(250, snapshot.lastWsCloseMs);
}

void test_peak_logs_stay_silent_at_info_level() {
    HttpServerHealthTracker::recordOpen();

    const auto snapshot = HttpServerHealthTracker::getSnapshot();
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.peakClients);
    TEST_ASSERT_EQUAL_UINT32(0, s_logs.size());
}

void test_forced_ws_removal_keeps_warning_visibility() {
    HttpServerHealthTracker::recordWsForcedRemoval(42);

    TEST_ASSERT_EQUAL_UINT32(1, s_logs.size());
    TEST_ASSERT_EQUAL(ESP_LOG_WARN, s_logs[0].level);
    TEST_ASSERT_EQUAL_STRING("Forced WebSocket client removal: fd=42 total=1", s_logs[0].message.c_str());
}

void test_counts_ws_queue_drops_with_timestamp_and_payload() {
    TEST_STUBS::ARDUINO::millisValue = 1234;
    HttpServerHealthTracker::recordWsQueueDrop(512);

    TEST_STUBS::ARDUINO::millisValue = 4321;
    HttpServerHealthTracker::recordWsQueueDrop(2048);

    const auto snapshot = HttpServerHealthTracker::getSnapshot();
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.wsQueueDrops);
    TEST_ASSERT_EQUAL_UINT32(4321, snapshot.lastWsQueueDropMs);
    TEST_ASSERT_EQUAL_UINT32(2048, snapshot.lastWsQueueDropPayload);
}

void test_counts_ws_heap_fallbacks_with_timestamp_and_max_payload() {
    TEST_STUBS::ARDUINO::millisValue = 1111;
    HttpServerHealthTracker::recordWsHeapFallback(9000);

    TEST_STUBS::ARDUINO::millisValue = 2222;
    HttpServerHealthTracker::recordWsHeapFallback(4096);

    const auto snapshot = HttpServerHealthTracker::getSnapshot();
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.wsHeapFallbacks);
    TEST_ASSERT_EQUAL_UINT32(2222, snapshot.lastWsHeapFallbackMs);
    TEST_ASSERT_EQUAL_UINT32(4096, snapshot.lastWsHeapFallbackPayload);
    TEST_ASSERT_EQUAL_UINT32(9000, snapshot.maxWsHeapFallbackPayload);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_counts_open_close_and_peak_clients);
    RUN_TEST(test_counts_forced_ws_removals);
    RUN_TEST(test_counts_websocket_session_lifecycle_separately_from_http_clients);
    RUN_TEST(test_peak_logs_stay_silent_at_info_level);
    RUN_TEST(test_forced_ws_removal_keeps_warning_visibility);
    RUN_TEST(test_counts_ws_queue_drops_with_timestamp_and_payload);
    RUN_TEST(test_counts_ws_heap_fallbacks_with_timestamp_and_max_payload);
    return UNITY_END();
}
