#include <unity.h>

#include <string>
#include <vector>

#include <ArduinoJson.h>

#include "../../src/system/logging/Logging.h"
#include "../../src/system/health/network/HttpServerHealthTracker.h"
#include "../../src/api/system/websocket/snapshots/SystemSnapshotTransport.cpp"

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
void HttpServerHealthTracker::recordWsQueueDrop(size_t payloadLen) { (void)payloadLen; }
void HttpServerHealthTracker::recordWsHeapFallback(size_t payloadLen) { (void)payloadLen; }
} // namespace SYSTEM::HEALTH

namespace TEST_STUBS::SNAPSHOT_TRANSPORT {
inline int lastFd = -1;
inline httpd_ws_type_t lastType = HTTPD_WS_TYPE_TEXT;
inline std::vector<uint8_t> lastPayload;
inline size_t lastReserveLen = 0;

inline void reset() {
    lastFd = -1;
    lastType = HTTPD_WS_TYPE_TEXT;
    lastPayload.clear();
    lastReserveLen = 0;
}
} // namespace TEST_STUBS::SNAPSHOT_TRANSPORT

namespace API::WEBSOCKET {
WsPayloadPool::WsPayloadPool(const char* logTag) : _logTag(logTag) {}
WsPayloadPool::~WsPayloadPool() = default;
bool WsPayloadPool::init(size_t slotCount, size_t slotSize) {
    _slotCount = slotCount;
    _slotSize = slotSize;
    return true;
}
void WsPayloadPool::deinit() {
    _slotCount = 0;
    _slotSize = 0;
}

WsClientManager::WsClientManager(const char* logTag,
                                 IWebSocketAuthenticator* authenticator,
                                 StateChangeCallback onStateChange,
                                 uint32_t sendTimeoutMs)
    : _logTag(logTag), _authenticator(authenticator), _onStateChange(onStateChange), _sendTimeoutMs(sendTimeoutMs) {}
WsClientManager::~WsClientManager() = default;
WsTaskQueue::WsTaskQueue(const char* logTag, ProcessCallback processCb, WsPayloadPool* pool)
    : _logTag(logTag), _processCb(processCb), _pool(pool) {}
WsTaskQueue::~WsTaskQueue() = default;
} // namespace API::WEBSOCKET

namespace API {
WebSocketBroadcaster::WebSocketBroadcaster(
    const char* logTag,
    IWebSocketAuthenticator* authenticator,
    StateChangeCallback onStateChange,
    uint32_t sendTimeoutMs)
    : _logTag(logTag), _pool(logTag), _clientMgr(logTag, authenticator, onStateChange, sendTimeoutMs), _taskQueue(logTag, nullptr, &_pool) {}

WebSocketBroadcaster::~WebSocketBroadcaster() = default;

esp_err_t WebSocketBroadcaster::handleHandshake(httpd_req_t* req) {
    (void)req;
    return ESP_OK;
}

void WebSocketBroadcaster::removeClient(int fd, bool triggerClose) {
    (void)fd;
    (void)triggerClose;
}

void WebSocketBroadcaster::broadcast(uint8_t* data, size_t len, httpd_ws_type_t type) {
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastType = type;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.assign(data, data + len);
}

void WebSocketBroadcaster::broadcast(int* fds, size_t count, uint8_t* data, size_t len, httpd_ws_type_t type) {
    TEST_ASSERT_EQUAL(1, static_cast<int>(count));
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastFd = fds ? fds[0] : -1;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastType = type;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.assign(data, data + len);
}

bool WebSocketBroadcaster::broadcastSerialized(size_t reserveLen, PayloadWriter writer, httpd_ws_type_t type) {
    std::vector<uint8_t> buffer(reserveLen);
    const size_t written = writer(buffer.data(), buffer.size());
    if (written == 0 || written > reserveLen) {
        return false;
    }
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastReserveLen = reserveLen;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastFd = -1;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastType = type;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.assign(buffer.begin(), buffer.begin() + written);
    return written > 0;
}

bool WebSocketBroadcaster::broadcastSerialized(int* fds, size_t count, size_t reserveLen, PayloadWriter writer, httpd_ws_type_t type) {
    TEST_ASSERT_EQUAL(1, static_cast<int>(count));
    std::vector<uint8_t> buffer(reserveLen);
    const size_t written = writer(buffer.data(), buffer.size());
    if (written == 0 || written > reserveLen) {
        return false;
    }
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastReserveLen = reserveLen;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastFd = fds ? fds[0] : -1;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastType = type;
    TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.assign(buffer.begin(), buffer.begin() + written);
    return written > 0;
}

void WebSocketBroadcaster::enableQueue(size_t queueSize, uint32_t stackSize, size_t payloadSlotSize) {
    (void)queueSize;
    (void)stackSize;
    (void)payloadSlotSize;
}

bool WebSocketBroadcaster::disableQueue() {
    return true;
}

bool WebSocketBroadcaster::hasClients() const {
    return true;
}

size_t WebSocketBroadcaster::getClientCount() const {
    return 1;
}

httpd_handle_t WebSocketBroadcaster::getServerHandle() const {
    return nullptr;
}

bool WebSocketBroadcaster::isQueueEnabled() const {
    return false;
}

size_t WebSocketBroadcaster::snapshotClients(int* outTargets, size_t maxCount) const {
    (void)outTargets;
    (void)maxCount;
    return 0;
}
} // namespace API

void setUp(void) {
    TEST_STUBS::SNAPSHOT_TRANSPORT::reset();
}

void tearDown(void) {}

void test_send_snapshot_doc_serializes_directly_into_reserved_payload() {
    API::WebSocketBroadcaster ws("test");

    SYSTEM::SpiRamJsonDocument doc(256);
    doc["type"] = "snapshot";
    doc["channel"] = "telemetry";
    doc["data"]["ok"] = true;

    TEST_ASSERT_TRUE(API::SYSTEM_WS::sendSnapshotDoc(&ws, 7, doc));
    TEST_ASSERT_EQUAL(7, TEST_STUBS::SNAPSHOT_TRANSPORT::lastFd);
    TEST_ASSERT_EQUAL(HTTPD_WS_TYPE_TEXT, TEST_STUBS::SNAPSHOT_TRANSPORT::lastType);
    TEST_ASSERT_FALSE(TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.empty());
    TEST_ASSERT_EQUAL(doc.requestedCapacityLimit(), TEST_STUBS::SNAPSHOT_TRANSPORT::lastReserveLen);

    std::string payload(
        reinterpret_cast<const char*>(TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.data()),
        TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.size());
    TEST_ASSERT_NOT_EQUAL(std::string::npos, payload.find("\"channel\":\"telemetry\""));
}

void test_send_snapshot_doc_supports_large_payloads_without_scratch_pool() {
    API::WebSocketBroadcaster ws("test");

    SYSTEM::SpiRamJsonDocument doc(256);
    doc["type"] = "snapshot";
    doc["channel"] = "system_status";
    doc["data"]["message"] = "this payload is larger than the scratch slot";

    TEST_ASSERT_TRUE(API::SYSTEM_WS::sendSnapshotDoc(&ws, 9, doc));
    TEST_ASSERT_EQUAL(9, TEST_STUBS::SNAPSHOT_TRANSPORT::lastFd);
    TEST_ASSERT_FALSE(TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.empty());
    TEST_ASSERT_EQUAL(doc.requestedCapacityLimit(), TEST_STUBS::SNAPSHOT_TRANSPORT::lastReserveLen);
}

void test_send_snapshot_doc_drops_payloads_that_exceed_reserve() {
    API::WebSocketBroadcaster ws("test");

    SYSTEM::SpiRamJsonDocument doc(64);
    doc["type"] = "snapshot";
    doc["channel"] = "telemetry";
    doc["data"]["message"] =
        "this message is intentionally longer than the requested websocket reserve";

    TEST_ASSERT_FALSE(API::SYSTEM_WS::sendSnapshotDoc(&ws, 11, doc));
    TEST_ASSERT_TRUE(TEST_STUBS::SNAPSHOT_TRANSPORT::lastPayload.empty());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_send_snapshot_doc_serializes_directly_into_reserved_payload);
    RUN_TEST(test_send_snapshot_doc_supports_large_payloads_without_scratch_pool);
    RUN_TEST(test_send_snapshot_doc_drops_payloads_that_exceed_reserve);
    return UNITY_END();
}
