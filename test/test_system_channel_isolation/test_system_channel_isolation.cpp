#include <unity.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

#include <freertos/task.h>
#include <WiFi.h>

float temperatureRead();

#include "../../src/api/common/websocket/WsPayloadPool.cpp"
#include "../../src/api/common/websocket/WsTaskQueue.cpp"
#include "../../src/api/common/websocket/WsClientManager.cpp"
#include "../../src/api/common/WebSocketBroadcaster.cpp"
#include "../../src/api/common/ChannelSubscriptions.cpp"
#include "../../src/api/system/broadcasters/SensorBroadcaster.cpp"

#define private public
#include "../../src/api/system/broadcasters/SystemStatusBroadcaster.cpp"
#undef private

namespace LOG {
Settings Logging::_settings;
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
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
} // namespace LOG

namespace SYSTEM::HEALTH {
void HttpServerHealthTracker::recordOpen() {}

void HttpServerHealthTracker::recordWsOpen() {}

void HttpServerHealthTracker::recordWsClose() {}

void HttpServerHealthTracker::recordWsForcedRemoval(int fd) {
    (void)fd;
}

void HttpServerHealthTracker::recordWsQueueDrop(size_t payloadLen) {
    (void)payloadLen;
}

void HttpServerHealthTracker::recordWsHeapFallback(size_t payloadLen) {
    (void)payloadLen;
}
} // namespace SYSTEM::HEALTH

WiFiClass WiFi;

namespace TEST_STUBS::HTTPD {
inline std::vector<int> sentFds;
inline std::vector<std::vector<uint8_t>> sentPayloads;
inline int lastClosedFd = -1;

inline void reset() {
    sentFds.clear();
    sentPayloads.clear();
    lastClosedFd = -1;
}
} // namespace TEST_STUBS::HTTPD

namespace TEST_STUBS::SENSORS {
inline SensorLoggingTask::UpdateCallback updateCallback = nullptr;
}

float temperatureRead() {
    return 24.5f;
}

bool SensorLoggingTask::setUpdateCallback(UpdateCallback callback) {
    TEST_STUBS::SENSORS::updateCallback = std::move(callback);
    return true;
}

namespace WIFISENSING {
void WifiSensingService::addSensingCallback(SensingCallback cb) {
    (void)cb;
}
} // namespace WIFISENSING

namespace MACROS {
void MacroService::setUpdateCallback(StateCallback cb) {
    (void)cb;
}
} // namespace MACROS

extern "C" esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {
    (void)r;
    (void)type;
    return ESP_OK;
}

extern "C" esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t len) {
    (void)r;
    (void)buf;
    (void)len;
    return ESP_OK;
}

extern "C" int httpd_req_to_sockfd(httpd_req_t* r) {
    return r ? r->sockfd : -1;
}

extern "C" esp_err_t httpd_ws_recv_frame(httpd_req_t* req, httpd_ws_frame_t* pkt, size_t max_len) {
    (void)req;
    (void)pkt;
    (void)max_len;
    return ESP_OK;
}

extern "C" esp_err_t httpd_ws_send_data(httpd_handle_t handle, int sockfd, httpd_ws_frame_t* pkt) {
    (void)handle;
    TEST_STUBS::HTTPD::sentFds.push_back(sockfd);
    TEST_STUBS::HTTPD::sentPayloads.emplace_back(
        pkt && pkt->payload ? pkt->payload : nullptr,
        pkt && pkt->payload ? pkt->payload + pkt->len : nullptr
    );
    return ESP_OK;
}

extern "C" void httpd_sess_trigger_close(httpd_handle_t handle, int sockfd) {
    (void)handle;
    TEST_STUBS::HTTPD::lastClosedFd = sockfd;
}

extern "C" void httpd_sess_update_lru_counter(httpd_handle_t handle, int sockfd) {
    (void)handle;
    (void)sockfd;
}

extern "C" int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return 0;
}

namespace {

httpd_req_t makeReq(int sockfd, httpd_handle_t handle = reinterpret_cast<httpd_handle_t>(0x1)) {
    httpd_req_t req{};
    req.method = HTTP_GET;
    req.handle = handle;
    req.sockfd = sockfd;
    return req;
}

void registerClient(API::WebSocketBroadcaster& broadcaster, int fd) {
    httpd_req_t req = makeReq(fd);
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleHandshake(&req));
}

void assertSinglePacketMagic(uint8_t expectedMagic) {
    TEST_ASSERT_EQUAL(1, static_cast<int>(TEST_STUBS::HTTPD::sentPayloads.size()));
    TEST_ASSERT_FALSE(TEST_STUBS::HTTPD::sentPayloads[0].empty());
    TEST_ASSERT_EQUAL_UINT8(expectedMagic, TEST_STUBS::HTTPD::sentPayloads[0][0]);
}

} // namespace

void setUp(void) {
    TEST_STUBS::HTTPD::reset();
    TEST_STUBS::SENSORS::updateCallback = nullptr;
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::WIFI::reset();
}

void tearDown(void) {}

void test_telemetry_live_packets_only_reach_telemetry_subscribers() {
    API::WebSocketBroadcaster socket("test");
    API::ChannelSubscriptions channels;
    PsychicHttpServer server;
    API::SensorBroadcaster broadcaster;

    registerClient(socket, 11);
    registerClient(socket, 12);

    channels.subscribe(11, API::ChannelSubscriptions::TELEMETRY);

    broadcaster.begin(&socket, &channels, &server, nullptr);
    TEST_ASSERT_NOT_NULL(TEST_STUBS::SENSORS::updateCallback);

    TEST_STUBS::ARDUINO::millisValue = 1000;
    SensorSnapshot snapshot{};
    snapshot.co2 = 640;
    snapshot.temp = 21.5f;
    snapshot.humid = 48.2f;
    snapshot.timestamp_ms = 123456;

    TEST_STUBS::SENSORS::updateCallback(snapshot, true);

    TEST_ASSERT_EQUAL(1, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_EQUAL(11, TEST_STUBS::HTTPD::sentFds[0]);
    assertSinglePacketMagic(0x54);
}

void test_macro_live_packets_only_reach_macro_subscribers() {
    API::WebSocketBroadcaster socket("test");
    API::ChannelSubscriptions channels;
    API::SystemStatusBroadcaster broadcaster;

    registerClient(socket, 21);
    registerClient(socket, 22);

    channels.subscribe(22, API::ChannelSubscriptions::MACROS);

    broadcaster.begin(&socket, &channels, nullptr, nullptr);

    TEST_STUBS::ARDUINO::millisValue = 1500;
    MACROS::MacroState state;
    state.status = MACROS::MacroStatus::RUNNING;
    state.currentLine = 7;
    state.startTime = 1000;
    state.currentScript = "demo.txt";

    broadcaster.broadcastMacroState(state);

    TEST_ASSERT_EQUAL(1, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_EQUAL(22, TEST_STUBS::HTTPD::sentFds[0]);
    assertSinglePacketMagic(0x4D);
}

void test_system_status_packets_remain_global_without_channel_subscription() {
    API::WebSocketBroadcaster socket("test");
    API::ChannelSubscriptions channels;
    API::SystemStatusBroadcaster broadcaster;

    registerClient(socket, 31);
    registerClient(socket, 32);

    broadcaster.begin(&socket, &channels, nullptr, nullptr);
    TEST_STUBS::ARDUINO::millisValue = API::WS_CLIENT_HANDSHAKE_GRACE_MS + 1;
    broadcaster.broadcastStatus();

    TEST_ASSERT_EQUAL(2, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_EQUAL(31, TEST_STUBS::HTTPD::sentFds[0]);
    TEST_ASSERT_EQUAL(32, TEST_STUBS::HTTPD::sentFds[1]);
    TEST_ASSERT_EQUAL(2, static_cast<int>(TEST_STUBS::HTTPD::sentPayloads.size()));
    TEST_ASSERT_FALSE(TEST_STUBS::HTTPD::sentPayloads[0].empty());
    TEST_ASSERT_FALSE(TEST_STUBS::HTTPD::sentPayloads[1].empty());
    TEST_ASSERT_EQUAL_UINT8(0xA5, TEST_STUBS::HTTPD::sentPayloads[0][0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5, TEST_STUBS::HTTPD::sentPayloads[1][0]);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_telemetry_live_packets_only_reach_telemetry_subscribers);
    RUN_TEST(test_macro_live_packets_only_reach_macro_subscribers);
    RUN_TEST(test_system_status_packets_remain_global_without_channel_subscription);
    return UNITY_END();
}
