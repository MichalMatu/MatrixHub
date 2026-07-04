#include <unity.h>

#include <cstring>
#include <string>

#include <Arduino.h>

#define private public
#include "../../src/api/common/ChannelSubscriptions.cpp"
#include "../../src/api/system/websocket/SystemWebsocketBroadcaster.cpp"
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

namespace TEST_STUBS::WS {
inline std::string incomingFrame;
inline size_t telemetrySnapshotCalls = 0;
inline size_t bleSnapshotCalls = 0;
inline size_t systemStatusSnapshotCalls = 0;
inline size_t airMouseSyncCalls = 0;
inline size_t bleSyncCalls = 0;
inline size_t notificationSyncCalls = 0;
inline int lastSnapshotFd = -1;
inline bool lastSnapshotHadBufferPool = false;
inline size_t lastQueueSize = 0;
inline uint32_t lastQueueStackSize = 0;
inline size_t lastQueuePayloadSlotSize = 0;
inline std::string lastRegisteredUri;
inline size_t removeClientCalls = 0;
inline int lastRemovedFd = -1;
inline bool lastRemoveTriggeredClose = false;

inline void reset() {
    incomingFrame.clear();
    telemetrySnapshotCalls = 0;
    bleSnapshotCalls = 0;
    systemStatusSnapshotCalls = 0;
    airMouseSyncCalls = 0;
    bleSyncCalls = 0;
    notificationSyncCalls = 0;
    lastSnapshotFd = -1;
    lastSnapshotHadBufferPool = false;
    lastQueueSize = 0;
    lastQueueStackSize = 0;
    lastQueuePayloadSlotSize = 0;
    lastRegisteredUri.clear();
    removeClientCalls = 0;
    lastRemovedFd = -1;
    lastRemoveTriggeredClose = false;
}
} // namespace TEST_STUBS::WS

namespace POWER {
void PowerManager::notifyActivity(const char* source) {
    (void)source;
}
} // namespace POWER

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
JwtAuthenticator::JwtAuthenticator(SecurityManager* securityManager, AuthenticationPredicate predicate)
    : _securityManager(securityManager), _predicate(predicate) {}

bool JwtAuthenticator::authenticate(httpd_req_t* req) {
    (void)req;
    return true;
}

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
    TEST_STUBS::WS::removeClientCalls++;
    TEST_STUBS::WS::lastRemovedFd = fd;
    TEST_STUBS::WS::lastRemoveTriggeredClose = triggerClose;
}

void WebSocketBroadcaster::broadcast(uint8_t* data, size_t len, httpd_ws_type_t type) {
    (void)data;
    (void)len;
    (void)type;
}

void WebSocketBroadcaster::broadcast(int* fds, size_t count, uint8_t* data, size_t len, httpd_ws_type_t type) {
    (void)fds;
    (void)count;
    (void)data;
    (void)len;
    (void)type;
}

void WebSocketBroadcaster::enableQueue(size_t queueSize, uint32_t stackSize, size_t payloadSlotSize) {
    TEST_STUBS::WS::lastQueueSize = queueSize;
    TEST_STUBS::WS::lastQueueStackSize = stackSize;
    TEST_STUBS::WS::lastQueuePayloadSlotSize = payloadSlotSize;
}

bool WebSocketBroadcaster::disableQueue() {
    return true;
}

bool WebSocketBroadcaster::hasClients() const {
    return false;
}

size_t WebSocketBroadcaster::getClientCount() const {
    return 0;
}

httpd_handle_t WebSocketBroadcaster::getServerHandle() const {
    return nullptr;
}

bool WebSocketBroadcaster::isQueueEnabled() const {
    return false;
}

bool WebSocketBroadcaster::broadcastSerialized(size_t reserveLen, PayloadWriter writer, httpd_ws_type_t type) {
    (void)reserveLen;
    (void)writer;
    (void)type;
    return true;
}

bool WebSocketBroadcaster::broadcastSerialized(int* fds, size_t count, size_t reserveLen, PayloadWriter writer, httpd_ws_type_t type) {
    (void)fds;
    (void)count;
    (void)reserveLen;
    (void)writer;
    (void)type;
    return true;
}

size_t WebSocketBroadcaster::snapshotClients(int* outTargets, size_t maxCount) const {
    (void)outTargets;
    (void)maxCount;
    return 0;
}

WsEndpointRuntime::WsEndpointRuntime(PsychicHttpServer* server,
                                     const char* uri,
                                     const char* logTag,
                                     IWebSocketAuthenticator* authenticator,
                                     WebSocketBroadcaster::StateChangeCallback onStateChange,
                                     uint32_t sendTimeoutMs)
    : _server(server),
      _uri(uri),
      _broadcaster(logTag, authenticator, onStateChange, sendTimeoutMs) {}

void WsEndpointRuntime::setFrameHandler(FrameHandler handler) {
    _frameHandler = std::move(handler);
}

void WsEndpointRuntime::setOpenCallback(ClientCallback callback) {
    _openCallback = std::move(callback);
}

void WsEndpointRuntime::setCleanupCallback(ClientCallback callback) {
    _cleanupCallback = std::move(callback);
}

void WsEndpointRuntime::setRequestCallback(RequestCallback callback) {
    _requestCallback = std::move(callback);
}

void WsEndpointRuntime::begin() {
    TEST_STUBS::WS::lastRegisteredUri = _uri ? _uri : "";
}

void WsEndpointRuntime::begin(size_t queueSize, uint32_t stackSize, size_t payloadSlotSize) {
    begin();
    _broadcaster.enableQueue(queueSize, stackSize, payloadSlotSize);
}

esp_err_t WsEndpointRuntime::handleRequest(httpd_req_t* req) {
    if (_requestCallback) {
        _requestCallback();
    }

    if (req && req->method == HTTP_GET) {
        const esp_err_t err = _broadcaster.handleHandshake(req);
        if (err == ESP_OK && _openCallback) {
            _openCallback(httpd_req_to_sockfd(req));
        }
        return err;
    }

    if (_frameHandler) {
        return _frameHandler(req, httpd_req_to_sockfd(req));
    }

    return _broadcaster.handleHandshake(req);
}

void WsEndpointRuntime::cleanupClient(int fd) {
    _broadcaster.removeClient(fd);
    if (_cleanupCallback) {
        _cleanupCallback(fd);
    }
}

WebSocketBroadcaster& WsEndpointRuntime::broadcaster() {
    return _broadcaster;
}

const WebSocketBroadcaster& WsEndpointRuntime::broadcaster() const {
    return _broadcaster;
}

AlarmBroadcaster::AlarmBroadcaster() = default;
void AlarmBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, ALARMS::AlarmService* alarmService) {
    (void)systemWs;
    (void)channels;
    (void)server;
    (void)alarmService;
}

AirMouseBroadcaster::AirMouseBroadcaster() = default;
AirMouseBroadcaster::~AirMouseBroadcaster() = default;
void AirMouseBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, AIRMOUSE::AirMouseService* airMouseService) {
    (void)systemWs;
    (void)channels;
    (void)server;
    (void)airMouseService;
}
void AirMouseBroadcaster::syncSubscriptionState() {
    TEST_STUBS::WS::airMouseSyncCalls++;
}

BleBroadcaster::BleBroadcaster() = default;
BleBroadcaster::~BleBroadcaster() = default;
void BleBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server) {
    (void)systemWs;
    (void)channels;
    (void)server;
}
void BleBroadcaster::syncSubscriptionState() {
    TEST_STUBS::WS::bleSyncCalls++;
}

SensorBroadcaster::SensorBroadcaster() = default;
SensorBroadcaster::~SensorBroadcaster() = default;
void SensorBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, WIFISENSING::WifiSensingService* wifiSensing) {
    (void)systemWs;
    (void)channels;
    (void)server;
    (void)wifiSensing;
}
void SensorBroadcaster::sendShellyEvent(const void* devicePtr) {
    (void)devicePtr;
}

SystemStatusBroadcaster::SystemStatusBroadcaster() = default;
SystemStatusBroadcaster::~SystemStatusBroadcaster() = default;
void SystemStatusBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, BLE::BleService* bleService, MACROS::MacroService* macroService) {
    (void)systemWs;
    (void)channels;
    (void)bleService;
    (void)macroService;
}
void SystemStatusBroadcaster::startTimer() {}
void SystemStatusBroadcaster::stopTimer() {}

void TelegramStatusBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, TELEGRAM::TelegramWorker* worker) {
    (void)systemWs;
    (void)channels;
    (void)server;
    (void)worker;
}
void TelegramStatusBroadcaster::sendSnapshot(int fd) {
    TEST_STUBS::WS::lastSnapshotFd = fd;
}

void NotificationBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, TELEGRAM::TelegramWorker* telegramWorker) {
    (void)systemWs;
    (void)channels;
    (void)server;
    (void)telegramWorker;
}
void NotificationBroadcaster::syncSubscriptionState() {
    TEST_STUBS::WS::notificationSyncCalls++;
}
void NotificationBroadcaster::sendSnapshot(int fd) {
    TEST_STUBS::WS::lastSnapshotFd = fd;
}
} // namespace API

namespace API::SYSTEM_WS {
void sendShellySnapshot(const SnapshotContext& ctx) {
    TEST_STUBS::WS::lastSnapshotHadBufferPool = false;
    TEST_STUBS::WS::lastSnapshotFd = ctx.fd;
}

void sendSensingSnapshot(const SnapshotContext& ctx) {
    TEST_STUBS::WS::lastSnapshotHadBufferPool = false;
    TEST_STUBS::WS::lastSnapshotFd = ctx.fd;
}

void sendTelemetrySnapshot(const SnapshotContext& ctx) {
    TEST_STUBS::WS::telemetrySnapshotCalls++;
    TEST_STUBS::WS::lastSnapshotHadBufferPool = false;
    TEST_STUBS::WS::lastSnapshotFd = ctx.fd;
}

void sendSystemStatusSnapshot(const SnapshotContext& ctx) {
    TEST_STUBS::WS::systemStatusSnapshotCalls++;
    TEST_STUBS::WS::lastSnapshotHadBufferPool = false;
    TEST_STUBS::WS::lastSnapshotFd = ctx.fd;
}

void sendBleSnapshot(const SnapshotContext& ctx) {
    TEST_STUBS::WS::bleSnapshotCalls++;
    TEST_STUBS::WS::lastSnapshotHadBufferPool = false;
    TEST_STUBS::WS::lastSnapshotFd = ctx.fd;
}

void sendAlarmsSnapshot(const SnapshotContext& ctx) {
    TEST_STUBS::WS::lastSnapshotHadBufferPool = false;
    TEST_STUBS::WS::lastSnapshotFd = ctx.fd;
}
} // namespace API::SYSTEM_WS

extern "C" esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t* uri_handler) {
    (void)handle;
    (void)uri_handler;
    return ESP_OK;
}

extern "C" int httpd_req_to_sockfd(httpd_req_t* req) {
    return req ? req->sockfd : -1;
}

extern "C" esp_err_t httpd_ws_recv_frame(httpd_req_t* req, httpd_ws_frame_t* pkt, size_t max_len) {
    (void)req;
    if (!pkt) {
        return ESP_FAIL;
    }

    if (max_len == 0) {
        pkt->len = TEST_STUBS::WS::incomingFrame.size();
        return ESP_OK;
    }

    if (max_len < TEST_STUBS::WS::incomingFrame.size()) {
        return ESP_FAIL;
    }

    if (pkt->payload && !TEST_STUBS::WS::incomingFrame.empty()) {
        memcpy(pkt->payload, TEST_STUBS::WS::incomingFrame.data(), TEST_STUBS::WS::incomingFrame.size());
    }
    pkt->len = TEST_STUBS::WS::incomingFrame.size();
    return ESP_OK;
}

extern "C" esp_err_t httpd_ws_send_data(httpd_handle_t handle, int sockfd, httpd_ws_frame_t* pkt) {
    (void)handle;
    (void)sockfd;
    (void)pkt;
    return ESP_OK;
}

extern "C" void httpd_sess_trigger_close(httpd_handle_t handle, int sockfd) {
    (void)handle;
    (void)sockfd;
}

extern "C" void httpd_sess_update_lru_counter(httpd_handle_t handle, int sockfd) {
    (void)handle;
    (void)sockfd;
}

namespace {

httpd_req_t makeWsFrameRequest(API::SystemWebsocketBroadcaster& broadcaster, int sockfd) {
    httpd_req_t req{};
    req.method = HTTP_POST;
    (void)broadcaster;
    req.sockfd = sockfd;
    req.handle = reinterpret_cast<httpd_handle_t>(0x1);
    return req;
}

void setIncomingFrame(const char* payload) {
    TEST_STUBS::WS::incomingFrame = payload ? payload : "";
}

} // namespace

void setUp(void) {
    TEST_STUBS::WS::reset();
}

void tearDown(void) {}

void test_first_subscribe_only_syncs_dynamic_channels() {
    PsychicHttpServer server;
    API::SystemApiBroadcastDeps deps;
    API::SystemWebsocketBroadcaster broadcaster(&server, nullptr, deps);
    httpd_req_t req = makeWsFrameRequest(broadcaster, 41);

    setIncomingFrame("{\"subscribe\":\"telemetry\"}");

    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));
    TEST_ASSERT_EQUAL(static_cast<size_t>(0), TEST_STUBS::WS::telemetrySnapshotCalls);
    TEST_ASSERT_EQUAL(-1, TEST_STUBS::WS::lastSnapshotFd);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::airMouseSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::bleSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::notificationSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), broadcaster._channels.getSubscriberCount(API::ChannelSubscriptions::TELEMETRY));
}

void test_duplicate_subscribe_stays_idempotent_without_sending_snapshot() {
    PsychicHttpServer server;
    API::SystemApiBroadcastDeps deps;
    API::SystemWebsocketBroadcaster broadcaster(&server, nullptr, deps);
    httpd_req_t req = makeWsFrameRequest(broadcaster, 42);

    setIncomingFrame("{\"subscribe\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    setIncomingFrame("{\"subscribe\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    TEST_ASSERT_EQUAL(static_cast<size_t>(0), TEST_STUBS::WS::telemetrySnapshotCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::airMouseSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::bleSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::notificationSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), broadcaster._channels.getSubscriberCount(API::ChannelSubscriptions::TELEMETRY));
}

void test_unsubscribe_resyncs_without_sending_another_snapshot() {
    PsychicHttpServer server;
    API::SystemApiBroadcastDeps deps;
    API::SystemWebsocketBroadcaster broadcaster(&server, nullptr, deps);
    httpd_req_t req = makeWsFrameRequest(broadcaster, 43);

    setIncomingFrame("{\"subscribe\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    setIncomingFrame("{\"unsubscribe\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    TEST_ASSERT_EQUAL(static_cast<size_t>(0), TEST_STUBS::WS::telemetrySnapshotCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), TEST_STUBS::WS::airMouseSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), TEST_STUBS::WS::bleSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), TEST_STUBS::WS::notificationSyncCalls);
    TEST_ASSERT_FALSE(broadcaster._channels.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
}

void test_explicit_snapshot_refresh_keeps_subscription_counts_stable() {
    PsychicHttpServer server;
    API::SystemApiBroadcastDeps deps;
    API::SystemWebsocketBroadcaster broadcaster(&server, nullptr, deps);
    httpd_req_t req = makeWsFrameRequest(broadcaster, 44);

    setIncomingFrame("{\"subscribe\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    setIncomingFrame("{\"snapshot\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::telemetrySnapshotCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::airMouseSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::bleSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::notificationSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), broadcaster._channels.getSubscriberCount(API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_EQUAL(44, TEST_STUBS::WS::lastSnapshotFd);
    TEST_ASSERT_FALSE(TEST_STUBS::WS::lastSnapshotHadBufferPool);
}

void test_oversize_frame_forces_close_and_cleans_channel_state() {
    PsychicHttpServer server;
    API::SystemApiBroadcastDeps deps;
    API::SystemWebsocketBroadcaster broadcaster(&server, nullptr, deps);
    httpd_req_t req = makeWsFrameRequest(broadcaster, 45);

    setIncomingFrame("{\"subscribe\":\"telemetry\"}");
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));
    TEST_ASSERT_EQUAL(static_cast<size_t>(1), broadcaster._channels.getSubscriberCount(API::ChannelSubscriptions::TELEMETRY));

    TEST_STUBS::WS::incomingFrame.assign(LIMITS::API::WS_MESSAGE_MAX_SIZE + 1, 'x');
    TEST_ASSERT_EQUAL(ESP_OK, broadcaster.handleFrame(&req, req.sockfd));

    TEST_ASSERT_EQUAL(static_cast<size_t>(1), TEST_STUBS::WS::removeClientCalls);
    TEST_ASSERT_EQUAL(45, TEST_STUBS::WS::lastRemovedFd);
    TEST_ASSERT_TRUE(TEST_STUBS::WS::lastRemoveTriggeredClose);
    TEST_ASSERT_FALSE(broadcaster._channels.hasSubscribers(API::ChannelSubscriptions::TELEMETRY));
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), TEST_STUBS::WS::airMouseSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), TEST_STUBS::WS::bleSyncCalls);
    TEST_ASSERT_EQUAL(static_cast<size_t>(2), TEST_STUBS::WS::notificationSyncCalls);
}

void test_begin_registers_system_endpoint_and_enables_fixed_system_ws_slots() {
    PsychicHttpServer server;
    API::SystemApiBroadcastDeps deps;
    API::SystemWebsocketBroadcaster broadcaster(&server, nullptr, deps);

    broadcaster.begin();

    TEST_ASSERT_EQUAL_STRING("/ws/system", TEST_STUBS::WS::lastRegisteredUri.c_str());
    TEST_ASSERT_EQUAL(LIMITS::API::SYSTEM_WS_QUEUE_SIZE, TEST_STUBS::WS::lastQueueSize);
    TEST_ASSERT_EQUAL(LIMITS::API::SYSTEM_WS_QUEUE_STACK, TEST_STUBS::WS::lastQueueStackSize);
    TEST_ASSERT_EQUAL(LIMITS::API::SYSTEM_WS_PAYLOAD_SLOT_SIZE, TEST_STUBS::WS::lastQueuePayloadSlotSize);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_first_subscribe_only_syncs_dynamic_channels);
    RUN_TEST(test_duplicate_subscribe_stays_idempotent_without_sending_snapshot);
    RUN_TEST(test_unsubscribe_resyncs_without_sending_another_snapshot);
    RUN_TEST(test_explicit_snapshot_refresh_keeps_subscription_counts_stable);
    RUN_TEST(test_oversize_frame_forces_close_and_cleans_channel_state);
    RUN_TEST(test_begin_registers_system_endpoint_and_enables_fixed_system_ws_slots);
    return UNITY_END();
}
