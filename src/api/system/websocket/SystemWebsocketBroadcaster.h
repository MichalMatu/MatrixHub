#pragma once

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../../../system/power/PowerManager.h"
#include "../../common/JwtAuthenticator.h"
#include "../../common/ChannelSubscriptions.h"
#include "../../common/WsEndpointRuntime.h"
#include "../broadcasters/AlarmBroadcaster.h"
#include "../broadcasters/AirMouseBroadcaster.h"
#include "../broadcasters/BleBroadcaster.h"
#include "../broadcasters/SensorBroadcaster.h"
#include "../broadcasters/SystemStatusBroadcaster.h"
#include "../broadcasters/TelegramStatusBroadcaster.h"
#include "../broadcasters/NotificationBroadcaster.h"
#include "../SystemApiFacets.h"

namespace BLE { class BleService; }
namespace SHELLY { class ShellyService; }
namespace MACROS { class MacroService; }
namespace TELEGRAM { class TelegramWorker; }
namespace AIRMOUSE { class AirMouseService; }
namespace ALARMS { class AlarmService; }
namespace WIFISENSING { class WifiSensingService; }
class WiFiSettingsService;

namespace API {

class SystemWebsocketBroadcaster {
public:
    SystemWebsocketBroadcaster(
        PsychicHttpServer* server,
        SecurityManager* securityManager,
        const SystemApiBroadcastDeps& deps);
    
    void begin();
    void cleanupClient(int fd);
    void sendShellyEvent(const void* devicePtr);

private:
    PsychicHttpServer* _server;
    POWER::PowerManager* _powerManager;
    SystemApiDependencies _sysDeps;
    JwtAuthenticator _wsAuthenticator;
    WsEndpointRuntime _endpoint;
    ChannelSubscriptions _channels;

    ALARMS::AlarmService* _alarmService;
    WIFISENSING::WifiSensingService* _wifiSensing;
    BLE::BleService* _bleService;
    SHELLY::ShellyService* _shellyService;
    MACROS::MacroService* _macroService;
    AIRMOUSE::AirMouseService* _airMouseService;
    WiFiSettingsService* _wifiSettingsService;
    TELEGRAM::TelegramWorker* _telegramWorker;

    AlarmBroadcaster _alarmBroadcaster;
    AirMouseBroadcaster _airMouseBroadcaster;
    BleBroadcaster _bleBroadcaster;
    SensorBroadcaster _sensorBroadcaster;
    SystemStatusBroadcaster _sysStatusBroadcaster;
    TelegramStatusBroadcaster _telegramBroadcaster;
    NotificationBroadcaster _notificationBroadcaster;

    esp_err_t handleFrame(httpd_req_t* req, int fd);
    esp_err_t rejectOversizeFrame(int fd, size_t frameLen, size_t maxLen);
    // Supports explicit snapshot refreshes without modifying subscription counts.
    bool tryHandleSnapshotRequest(int fd, const char* msg, size_t len);
    void syncDynamicChannels();
    void sendChannelSnapshot(int fd, ChannelSubscriptions::Channel ch);
};

} // namespace API
