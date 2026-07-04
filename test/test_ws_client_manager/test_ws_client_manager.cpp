#include <unity.h>
#include <chrono>
#include <deque>
#include <thread>
#include <vector>

#include <freertos/task.h>

#define private public
#include "../../src/api/common/websocket/WsClientManager.cpp"
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
inline int s_recordOpenCalls = 0;
inline int s_recordWsOpenCalls = 0;
inline int s_recordWsCloseCalls = 0;

void HttpServerHealthTracker::recordOpen() {
    s_recordOpenCalls++;
}

void HttpServerHealthTracker::recordWsOpen() {
    s_recordWsOpenCalls++;
}

void HttpServerHealthTracker::recordWsClose() {
    s_recordWsCloseCalls++;
}

void HttpServerHealthTracker::recordWsForcedRemoval(int fd) {
    (void)fd;
}
}  // namespace SYSTEM::HEALTH

namespace TEST_STUBS::HTTPD {
inline std::deque<esp_err_t> sendResults;
inline std::vector<int> sentFds;
inline std::vector<size_t> sentLens;
inline int lastClosedFd = -1;
inline int lastLruFd = -1;
inline int lastSetsockoptFd = -1;
inline long lastTimeoutSec = -1;
inline long lastTimeoutUsec = -1;
inline size_t lastRecvMaxLen = 0;

inline void reset() {
    sendResults.clear();
    sentFds.clear();
    sentLens.clear();
    lastClosedFd = -1;
    lastLruFd = -1;
    lastSetsockoptFd = -1;
    lastTimeoutSec = -1;
    lastTimeoutUsec = -1;
    lastRecvMaxLen = 0;
}
} // namespace TEST_STUBS::HTTPD

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
    TEST_STUBS::HTTPD::lastRecvMaxLen = max_len;
    return ESP_OK;
}

extern "C" esp_err_t httpd_ws_send_data(httpd_handle_t handle, int sockfd, httpd_ws_frame_t* pkt) {
    (void)handle;
    TEST_STUBS::HTTPD::sentFds.push_back(sockfd);
    TEST_STUBS::HTTPD::sentLens.push_back(pkt ? pkt->len : 0);
    if (!TEST_STUBS::HTTPD::sendResults.empty()) {
        esp_err_t result = TEST_STUBS::HTTPD::sendResults.front();
        TEST_STUBS::HTTPD::sendResults.pop_front();
        return result;
    }
    return ESP_OK;
}

extern "C" void httpd_sess_trigger_close(httpd_handle_t handle, int sockfd) {
    (void)handle;
    TEST_STUBS::HTTPD::lastClosedFd = sockfd;
}

extern "C" void httpd_sess_update_lru_counter(httpd_handle_t handle, int sockfd) {
    (void)handle;
    TEST_STUBS::HTTPD::lastLruFd = sockfd;
}

extern "C" int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    (void)level;
    (void)optname;
    (void)optlen;
    TEST_STUBS::HTTPD::lastSetsockoptFd = sockfd;
    const auto* tv = static_cast<const timeval*>(optval);
    if (tv) {
        TEST_STUBS::HTTPD::lastTimeoutSec = tv->tv_sec;
        TEST_STUBS::HTTPD::lastTimeoutUsec = tv->tv_usec;
    }
    return 0;
}

namespace {

class TestAuthenticator final : public API::IWebSocketAuthenticator {
public:
    explicit TestAuthenticator(bool allow) : _allow(allow) {}

    bool authenticate(httpd_req_t* req) override {
        (void)req;
        return _allow;
    }

private:
    bool _allow;
};

std::vector<bool> g_stateChanges;

void resetState() {
    TEST_STUBS::HTTPD::reset();
    g_stateChanges.clear();
    SYSTEM::HEALTH::s_recordOpenCalls = 0;
    SYSTEM::HEALTH::s_recordWsOpenCalls = 0;
    SYSTEM::HEALTH::s_recordWsCloseCalls = 0;
}

httpd_req_t makeReq(httpd_method_t method, int sockfd, httpd_handle_t handle = reinterpret_cast<httpd_handle_t>(0x1)) {
    httpd_req_t req{};
    req.method = method;
    req.handle = handle;
    req.sockfd = sockfd;
    return req;
}

void markClientReady(API::WEBSOCKET::WsClientManager& mgr, int fd) {
    auto it = mgr._clients.find(fd);
    TEST_ASSERT_TRUE(it != mgr._clients.end());
    it->second.readyAtMs = 0;
}

} // namespace

void setUp(void) {
    resetState();
}
void tearDown(void) {}

void test_handshake_registers_first_client_and_sets_timeout() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, [](bool hasClients) {
        g_stateChanges.push_back(hasClients);
    }, 1500);

    httpd_req_t req = makeReq(HTTP_GET, 7);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&req));
    TEST_ASSERT_TRUE(mgr.hasClients());
    TEST_ASSERT_EQUAL(1, mgr.getClientCount());
    TEST_ASSERT_EQUAL(1, static_cast<int>(g_stateChanges.size()));
    TEST_ASSERT_TRUE(g_stateChanges[0]);
    TEST_ASSERT_EQUAL(7, TEST_STUBS::HTTPD::lastSetsockoptFd);
    TEST_ASSERT_EQUAL(1, TEST_STUBS::HTTPD::lastTimeoutSec);
    TEST_ASSERT_EQUAL(500000, TEST_STUBS::HTTPD::lastTimeoutUsec);
    TEST_ASSERT_EQUAL(0, SYSTEM::HEALTH::s_recordOpenCalls);
    TEST_ASSERT_EQUAL(1, SYSTEM::HEALTH::s_recordWsOpenCalls);
    TEST_ASSERT_EQUAL(0, SYSTEM::HEALTH::s_recordWsCloseCalls);
}

void test_ws_health_counters_follow_registered_clients_without_counting_duplicate_handshakes() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t reqA = makeReq(HTTP_GET, 71);
    httpd_req_t reqADuplicate = makeReq(HTTP_GET, 71);
    httpd_req_t reqB = makeReq(HTTP_GET, 72);

    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqA));
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqADuplicate));
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqB));
    TEST_ASSERT_EQUAL(2, mgr.getClientCount());
    TEST_ASSERT_EQUAL(2, SYSTEM::HEALTH::s_recordWsOpenCalls);

    mgr.removeClient(71);
    mgr.removeClient(71);
    TEST_ASSERT_EQUAL(1, mgr.getClientCount());
    TEST_ASSERT_EQUAL(1, SYSTEM::HEALTH::s_recordWsCloseCalls);

    mgr.removeClient(72);
    TEST_ASSERT_FALSE(mgr.hasClients());
    TEST_ASSERT_EQUAL(2, SYSTEM::HEALTH::s_recordWsCloseCalls);
}

void test_handshake_auth_failure_closes_socket() {
    TestAuthenticator auth(false);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t req = makeReq(HTTP_GET, 9);
    TEST_ASSERT_EQUAL(ESP_FAIL, mgr.handleHandshake(&req));
    TEST_ASSERT_FALSE(mgr.hasClients());
    TEST_ASSERT_EQUAL(9, TEST_STUBS::HTTPD::lastClosedFd);
}

void test_soft_errors_require_threshold_before_removal() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, [](bool hasClients) {
        g_stateChanges.push_back(hasClients);
    }, 500);

    httpd_req_t req = makeReq(HTTP_GET, 11);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&req));
    markClientReady(mgr, 11);

    uint8_t payload[4] = {1, 2, 3, 4};
    for (int i = 0; i < API::WEBSOCKET::WsClientManager::MAX_SEND_FAILURES - 1; i++) {
        TEST_STUBS::HTTPD::sendResults.push_back(ESP_ERR_TIMEOUT);
        mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY);
    }

    TEST_ASSERT_TRUE(mgr.hasClients());
    TEST_ASSERT_EQUAL(-1, TEST_STUBS::HTTPD::lastClosedFd);

    TEST_STUBS::HTTPD::sendResults.push_back(ESP_ERR_TIMEOUT);
    mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY);

    TEST_ASSERT_FALSE(mgr.hasClients());
    TEST_ASSERT_EQUAL(11, TEST_STUBS::HTTPD::lastClosedFd);
    TEST_ASSERT_EQUAL(2, static_cast<int>(g_stateChanges.size()));
    TEST_ASSERT_FALSE(g_stateChanges.back());
}

void test_hard_error_removes_immediately() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, [](bool hasClients) {
        g_stateChanges.push_back(hasClients);
    }, 500);

    httpd_req_t req = makeReq(HTTP_GET, 13);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&req));
    markClientReady(mgr, 13);

    uint8_t payload[2] = {9, 9};
    TEST_STUBS::HTTPD::sendResults.push_back(ESP_ERR_NOT_FOUND);
    mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY);

    TEST_ASSERT_FALSE(mgr.hasClients());
    TEST_ASSERT_EQUAL(13, TEST_STUBS::HTTPD::lastClosedFd);
    TEST_ASSERT_EQUAL(2, static_cast<int>(g_stateChanges.size()));
    TEST_ASSERT_FALSE(g_stateChanges.back());
}

void test_targeted_broadcast_sends_only_connected_targets() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t reqA = makeReq(HTTP_GET, 21);
    httpd_req_t reqB = makeReq(HTTP_GET, 22);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqA));
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqB));
    markClientReady(mgr, 21);
    markClientReady(mgr, 22);

    int targets[3] = {22, 999, 21};
    uint8_t payload[3] = {1, 2, 3};
    mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY, targets, 3);

    TEST_ASSERT_EQUAL(2, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_EQUAL(22, TEST_STUBS::HTTPD::sentFds[0]);
    TEST_ASSERT_EQUAL(21, TEST_STUBS::HTTPD::sentFds[1]);
}

void test_broadcast_skips_clients_during_handshake_grace() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t req = makeReq(HTTP_GET, 23);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&req));

    uint8_t payload[3] = {1, 2, 3};
    mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY);
    TEST_ASSERT_EQUAL(0, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_TRUE(mgr.hasClients());

    markClientReady(mgr, 23);
    mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY);
    TEST_ASSERT_EQUAL(1, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_EQUAL(23, TEST_STUBS::HTTPD::sentFds[0]);
}

void test_has_clients_uses_atomic_cache_when_lock_is_busy() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t req = makeReq(HTTP_GET, 31);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&req));
    TEST_ASSERT_TRUE(mgr.hasClients());

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mgr._lock, portMAX_DELAY));
    TEST_ASSERT_TRUE(mgr.hasClients());
    TEST_ASSERT_EQUAL(1, mgr.getClientCount());
    xSemaphoreGive(mgr._lock);
}

void test_snapshot_clients_returns_connected_fds_in_registration_order() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t reqA = makeReq(HTTP_GET, 51);
    httpd_req_t reqB = makeReq(HTTP_GET, 52);
    httpd_req_t reqC = makeReq(HTTP_GET, 53);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqA));
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqB));
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&reqC));

    int snapshot[4] = {0};
    const size_t count = mgr.snapshotClients(snapshot, 4);

    TEST_ASSERT_EQUAL(static_cast<size_t>(3), count);
    TEST_ASSERT_EQUAL(51, snapshot[0]);
    TEST_ASSERT_EQUAL(52, snapshot[1]);
    TEST_ASSERT_EQUAL(53, snapshot[2]);
}

void test_broadcast_waits_for_lock_instead_of_dropping_frame() {
    TestAuthenticator auth(true);
    API::WEBSOCKET::WsClientManager mgr("test", &auth, nullptr, 500);

    httpd_req_t req = makeReq(HTTP_GET, 41);
    TEST_ASSERT_EQUAL(ESP_OK, mgr.handleHandshake(&req));
    markClientReady(mgr, 41);

    uint8_t payload[3] = {7, 8, 9};
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mgr._lock, portMAX_DELAY));

    std::thread sender([&]() {
        mgr.performBroadcast(payload, sizeof(payload), HTTPD_WS_TYPE_BINARY);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    TEST_ASSERT_EQUAL(0, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    xSemaphoreGive(mgr._lock);

    sender.join();

    TEST_ASSERT_EQUAL(1, static_cast<int>(TEST_STUBS::HTTPD::sentFds.size()));
    TEST_ASSERT_EQUAL(41, TEST_STUBS::HTTPD::sentFds[0]);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_handshake_registers_first_client_and_sets_timeout);
    RUN_TEST(test_ws_health_counters_follow_registered_clients_without_counting_duplicate_handshakes);
    RUN_TEST(test_handshake_auth_failure_closes_socket);
    RUN_TEST(test_soft_errors_require_threshold_before_removal);
    RUN_TEST(test_hard_error_removes_immediately);
    RUN_TEST(test_targeted_broadcast_sends_only_connected_targets);
    RUN_TEST(test_broadcast_skips_clients_during_handshake_grace);
    RUN_TEST(test_has_clients_uses_atomic_cache_when_lock_is_busy);
    RUN_TEST(test_snapshot_clients_returns_connected_fds_in_registration_order);
    RUN_TEST(test_broadcast_waits_for_lock_instead_of_dropping_frame);
    return UNITY_END();
}
