#include "SystemWebsocketBroadcaster.h"
#include "snapshots/SystemSnapshots.h"

#include <ArduinoJson.h>

#include "../../../config/System.h"
#include "../../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "SystemWS"

namespace API {

SystemWebsocketBroadcaster::SystemWebsocketBroadcaster(
    PsychicHttpServer* server,
    SecurityManager* securityManager,
    const SystemApiBroadcastDeps& deps)
    : _server(server)
    , _powerManager(deps.powerManager)
    , _sysDeps(deps.systemDeps)
    , _wsAuthenticator(securityManager)
    , _endpoint(server, "/ws/system", "WS-System", &_wsAuthenticator, [this](bool start) {
          // The small binary system heartbeat is endpoint-wide rather than
          // channel-scoped because the app shell/top-nav is present across the
          // UI and uses /ws/system liveness on every screen. Start it whenever
          // the endpoint transitions between zero and non-zero clients, even if
          // a given screen only subscribes to feature channels.
          if (start) {
              _sysStatusBroadcaster.startTimer();
          } else {
              _sysStatusBroadcaster.stopTimer();
          }
      })
    , _alarmService(deps.alarmService)
    , _wifiSensing(deps.wifiSensingService)
    , _bleService(deps.bleService)
    , _shellyService(deps.shellyService)
    , _macroService(deps.macroService)
    , _airMouseService(deps.airMouseService)
    , _wifiSettingsService(deps.wifiSettingsService)
    , _telegramWorker(deps.telegramWorker) {
    // /ws/system keeps the channel multiplexer model, but its transport
    // lifecycle now comes from the shared runtime instead of bespoke endpoint
    // code. If this starts diverging again, it is a sign the abstraction is
    // leaking and should be fixed centrally.
    _endpoint.setFrameHandler([this](httpd_req_t* req, int fd) {
        return handleFrame(req, fd);
    });
    _endpoint.setRequestCallback([this]() {
        if (_powerManager) {
            _powerManager->notifyActivity("ws/system");
        }
    });
    _endpoint.setCleanupCallback([this](int fd) {
        _channels.unsubscribeAll(fd);
        syncDynamicChannels();
    });
}

void SystemWebsocketBroadcaster::begin() {
    // Fixed slots cover the typical /ws/system traffic mix: 1 Hz live status
    // packets plus most on-demand snapshots. The explicit size is deliberate so
    // we stop using variable-size allocations for the dominant path.
    _endpoint.begin(
        LIMITS::API::SYSTEM_WS_QUEUE_SIZE,
        LIMITS::API::SYSTEM_WS_QUEUE_STACK,
        LIMITS::API::SYSTEM_WS_PAYLOAD_SLOT_SIZE);

    // ChannelSubscriptions intentionally remains unique to /ws/system. CSI and
    // USB terminal still use dedicated protocols; only the transport/runtime is
    // shared across endpoints.
    _sysStatusBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _bleService, _macroService);
    _sensorBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _server, _wifiSensing);
    _airMouseBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _server, _airMouseService);
    _bleBroadcaster.setBleService(_bleService);
    _bleBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _server);
    _alarmBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _server, _alarmService);
    _telegramBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _server, _telegramWorker); 
    _notificationBroadcaster.begin(&_endpoint.broadcaster(), &_channels, _server, _telegramWorker);
}

void SystemWebsocketBroadcaster::sendShellyEvent(const void* devicePtr) {
    _sensorBroadcaster.sendShellyEvent(devicePtr);
}

esp_err_t SystemWebsocketBroadcaster::handleFrame(httpd_req_t* req, int fd) {
    httpd_ws_frame_t ws_pkt{};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;
    
    if (ws_pkt.len == 0) return ESP_OK;

    // httpd_ws_recv_frame(..., 0) only peeks the frame length. If we silently
    // ignore an oversized client frame here, the unread payload stays queued on
    // the socket and the next receive can become desynchronized. Treat that as
    // invalid client input and close the session instead of attempting an
    // unbounded drain into RAM.
    if (ws_pkt.len > LIMITS::API::WS_MESSAGE_MAX_SIZE) {
        return rejectOversizeFrame(fd, ws_pkt.len, LIMITS::API::WS_MESSAGE_MAX_SIZE);
    }
    
    char buf[LIMITS::API::WS_MESSAGE_MAX_SIZE + 1];
    ws_pkt.payload = reinterpret_cast<uint8_t*>(buf);
    
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) return ret;
    
    buf[ws_pkt.len] = '\0';
    
    ChannelSubscriptions::Channel ch = ChannelSubscriptions::NONE;
    bool isSubscribe = false;
    bool changed = false;
    if (_channels.handleMessage(fd, buf, ws_pkt.len, &ch, &isSubscribe, &changed)) {
        if (changed && ch != ChannelSubscriptions::NONE) {
            syncDynamicChannels();
        }
        if (isSubscribe && changed) {
            // Subscribe now owns only backend channel state and dynamic stream
            // toggles. The frontend asks for rich JSON state explicitly through
            // {"snapshot":"..."} so lease ownership and snapshot freshness keep
            // separate, predictable semantics.
        }
        return ESP_OK;
    }

    (void)tryHandleSnapshotRequest(fd, buf, ws_pkt.len);

    return ESP_OK;
}

esp_err_t SystemWebsocketBroadcaster::rejectOversizeFrame(int fd,
                                                          size_t frameLen,
                                                          size_t maxLen) {
    LOGW("Closing /ws/system fd %d after oversize frame (%u > %u bytes)",
         fd,
         static_cast<unsigned>(frameLen),
         static_cast<unsigned>(maxLen));

    // Proactive cleanup keeps channel ref-counts and dynamic stream toggles in
    // sync immediately; the forced socket close notification can arrive later.
    _endpoint.broadcaster().removeClient(fd, true);
    _channels.unsubscribeAll(fd);
    syncDynamicChannels();
    return ESP_OK;
}

bool SystemWebsocketBroadcaster::tryHandleSnapshotRequest(int fd,
                                                          const char* msg,
                                                          size_t len) {
    if (!msg || len == 0) {
        return false;
    }

    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::CHANNEL_SUBSCRIPTIONS);
    const DeserializationError error = deserializeJson(doc, msg, len);
    if (error || doc.overflowed()) {
        return false;
    }

    if (!doc["snapshot"].is<const char*>()) {
        return false;
    }

    const char* channelName = doc["snapshot"].as<const char*>();
    const ChannelSubscriptions::Channel ch =
        ChannelSubscriptions::channelFromName(channelName);
    if (ch == ChannelSubscriptions::NONE) {
        LOGW("Unknown snapshot channel: %s", channelName ? channelName : "(null)");
        return false;
    }

    // Snapshot refresh is separate from subscription ownership so the frontend
    // can rehydrate state without inflating subscriber counts or resyncing twice.
    // TODO: Once system_status gets backend-side push-on-change, keep this path
    // as an explicit "force refresh" escape hatch rather than removing it.
    sendChannelSnapshot(fd, ch);
    return true;
}

void SystemWebsocketBroadcaster::syncDynamicChannels() {
    _airMouseBroadcaster.syncSubscriptionState();
    _bleBroadcaster.syncSubscriptionState();
    _notificationBroadcaster.syncSubscriptionState();
}

void SystemWebsocketBroadcaster::cleanupClient(int fd) {
    _endpoint.cleanupClient(fd);
}

void SystemWebsocketBroadcaster::sendChannelSnapshot(int fd, ChannelSubscriptions::Channel ch) {
    SYSTEM_WS::SnapshotContext ctx;
    // Snapshots now write directly into queue-owned payload buffers. If someone
    // reintroduces an extra scratch buffer here, that would be a regression
    // against the current "serialize once" design.
    // Today snapshots are targeted at one fd at a time. If we later broadcast
    // rich system_status updates to all subscribers, prefer extending the shared
    // snapshot transport to multi-target sends instead of rebuilding JSON once
    // per client.
    ctx.ws = &_endpoint.broadcaster();
    ctx.fd = fd;
    ctx.sysDeps = &_sysDeps;
    ctx.alarmService = _alarmService;
    ctx.wifiSensing = _wifiSensing;
    ctx.bleService = _bleService;
    ctx.shellyService = _shellyService;
    ctx.wifiSettingsService = _wifiSettingsService;

    switch (ch) {
        case ChannelSubscriptions::SHELLY: SYSTEM_WS::sendShellySnapshot(ctx); break;
        case ChannelSubscriptions::ALARMS: SYSTEM_WS::sendAlarmsSnapshot(ctx); break;
        case ChannelSubscriptions::BLE: SYSTEM_WS::sendBleSnapshot(ctx); break;
        case ChannelSubscriptions::SENSING: SYSTEM_WS::sendSensingSnapshot(ctx); break;
        case ChannelSubscriptions::TELEMETRY: SYSTEM_WS::sendTelemetrySnapshot(ctx); break;
        case ChannelSubscriptions::SYSTEM_STATUS: SYSTEM_WS::sendSystemStatusSnapshot(ctx); break;
        case ChannelSubscriptions::AIRMOUSE: break;
        case ChannelSubscriptions::TELEGRAM: _telegramBroadcaster.sendSnapshot(fd); break;
        case ChannelSubscriptions::NOTIF_STATS: _notificationBroadcaster.sendSnapshot(fd); break;
        default: break;
    }
}

} // namespace API
