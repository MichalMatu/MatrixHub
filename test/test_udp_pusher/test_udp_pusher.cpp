#include <unity.h>

#include <chrono>
#include <thread>

#include "../../test/stubs/WiFi.h"
#include "../../test/stubs/WiFiUdp.h"
#include "../../test/stubs/freertos/semphr.h"
#include "../../test/stubs/freertos/task.h"

#include "../../src/sensors/SensorLoggingTask.h"
#define private public
#include "../../src/udp/UdpPusher.h"
#undef private

#include "../../src/system/logging/Logging.h"
#include "../../src/system/rtc/RtcConfig.h"

namespace LOG {

Settings Logging::_settings{};

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::logSection(const char* title) {
    (void)title;
}

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

namespace RTC {

ConfigStore mockStore{};
ConfigStore* store = &mockStore;
RtcRuntimeStats runtimeStats{};

const ConfigStore& getConfig() {
    return mockStore;
}

ConfigStore getConfigSafeCopy() {
    return mockStore;
}

ConfigStore& getMutableConfig() {
    return mockStore;
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(mockStore);
}

SemaphoreHandle_t getLock() {
    static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    return lock;
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    updater(mockStore);
    return true;
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater, TickType_t timeoutTicks) {
    (void)timeoutTicks;
    updater(mockStore);
    return true;
}

void markValid() {}

}  // namespace RTC

WiFiClass WiFi;

namespace {

SensorSnapshot gSnapshot{};

void configureValidUdp() {
    RTC::mockStore = RTC::ConfigStore{};
    RTC::mockStore.udpPusher.enabled = true;
    strncpy(RTC::mockStore.udpPusher.host, "telegraf.local", sizeof(RTC::mockStore.udpPusher.host));
    RTC::mockStore.udpPusher.host[sizeof(RTC::mockStore.udpPusher.host) - 1] = '\0';
    RTC::mockStore.udpPusher.port = 8094;
    RTC::mockStore.udpPusher.format = RTC::UdpFormat::Json;
    RTC::mockStore.udpPusher.intervalMs = 1000;
}

void resetStubs() {
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::resetSemaphoreStubs();
    TEST_STUBS::WIFI::reset();
    TEST_STUBS::WIFIUDP::reset();
    RTC::runtimeStats = {};
    configureValidUdp();
    gSnapshot = SensorSnapshot{};
    gSnapshot.timestamp_ms = 123456;
    gSnapshot.co2 = 678;
    gSnapshot.temp = 24.5f;
    gSnapshot.humid = 45.2f;
    gSnapshot.seq = 7;
}

}  // namespace

SensorSnapshot SensorLoggingTask::getLastGoodSnapshot() {
    return gSnapshot;
}

#include "../../src/udp/UdpPacketFormatter.cpp"
#define private public
#include "../../src/udp/UdpPusher.cpp"
#undef private

void setUp(void) {
    resetStubs();
}

void tearDown(void) {}

void test_push_now_returns_wifi_disconnected_when_wifi_is_down() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFI::status = 0;
    TEST_STUBS::WIFI::connected = false;

    UDPPUSH::UdpPusher pusher;
    pusher.begin();

    const auto result = pusher.pushNow();

    TEST_ASSERT_EQUAL(
        static_cast<int>(UDPPUSH::UdpPusher::PushNowResult::WifiDisconnected),
        static_cast<int>(result));
}

void test_push_now_returns_send_failed_when_begin_packet_fails() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFIUDP::beginPacketResult = false;

    UDPPUSH::UdpPusher pusher;
    pusher.begin();

    const auto result = pusher.pushNow();

    TEST_ASSERT_EQUAL(
        static_cast<int>(UDPPUSH::UdpPusher::PushNowResult::SendFailed),
        static_cast<int>(result));
    TEST_ASSERT_EQUAL(1, static_cast<int>(TEST_STUBS::WIFIUDP::beginCalls));
}

void test_push_now_returns_send_failed_when_end_packet_fails() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFIUDP::endPacketResult = false;

    UDPPUSH::UdpPusher pusher;
    pusher.begin();

    const auto result = pusher.pushNow();

    TEST_ASSERT_EQUAL(
        static_cast<int>(UDPPUSH::UdpPusher::PushNowResult::SendFailed),
        static_cast<int>(result));
    TEST_ASSERT_EQUAL(1, static_cast<int>(RTC::runtimeStats.udpFailed));
}

void test_push_now_returns_sent_when_udp_send_succeeds() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;

    UDPPUSH::UdpPusher pusher;
    pusher.begin();

    const auto result = pusher.pushNow();

    TEST_ASSERT_EQUAL(
        static_cast<int>(UDPPUSH::UdpPusher::PushNowResult::Sent),
        static_cast<int>(result));
    TEST_ASSERT_EQUAL(1, static_cast<int>(RTC::runtimeStats.udpSent));
    TEST_ASSERT_EQUAL_STRING("telegraf.local", TEST_STUBS::WIFIUDP::lastHost.c_str());
    TEST_ASSERT_EQUAL(8094, TEST_STUBS::WIFIUDP::lastPort);
}

void test_update_and_push_now_do_not_run_send_concurrently_when_worker_start_fails() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;
    TEST_STUBS::WIFIUDP::sendDelayMs = 25;

    UDPPUSH::UdpPusher pusher;
    pusher.begin();
    TEST_STUBS::ARDUINO::millisValue = INTEGRATION::UDP::STARTUP_DELAY_MS;

    UDPPUSH::UdpPusher::PushNowResult pushResult = UDPPUSH::UdpPusher::PushNowResult::SendFailed;

    std::thread updateThread([&]() {
        pusher.update();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    std::thread pushThread([&]() {
        pushResult = pusher.pushNow();
    });

    updateThread.join();
    pushThread.join();

    TEST_ASSERT_EQUAL(2, static_cast<int>(TEST_STUBS::WIFIUDP::endCalls));
    TEST_ASSERT_EQUAL(1, TEST_STUBS::WIFIUDP::maxConcurrentSends.load());
    TEST_ASSERT_EQUAL(
        static_cast<int>(UDPPUSH::UdpPusher::PushNowResult::Sent),
        static_cast<int>(pushResult));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_push_now_returns_wifi_disconnected_when_wifi_is_down);
    RUN_TEST(test_push_now_returns_send_failed_when_begin_packet_fails);
    RUN_TEST(test_push_now_returns_send_failed_when_end_packet_fails);
    RUN_TEST(test_push_now_returns_sent_when_udp_send_succeeds);
    RUN_TEST(test_update_and_push_now_do_not_run_send_concurrently_when_worker_start_fails);
    return UNITY_END();
}
