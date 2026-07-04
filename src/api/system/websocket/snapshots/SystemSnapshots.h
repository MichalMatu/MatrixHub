#pragma once

#include "../../../../system/utils/json/JsonResponseWriter.h"
#include "../../../common/WebSocketBroadcaster.h"

class WiFiSettingsService;

namespace ALARMS {
class AlarmService;
}

namespace BLE {
class BleService;
}

namespace SHELLY {
class ShellyService;
}

namespace WIFISENSING {
class WifiSensingService;
}

namespace API {

struct SystemApiDependencies;

namespace SYSTEM_WS {

struct SnapshotContext {
    WebSocketBroadcaster* ws = nullptr;
    int fd = -1;
    const SystemApiDependencies* sysDeps = nullptr;
    ALARMS::AlarmService* alarmService = nullptr;
    WIFISENSING::WifiSensingService* wifiSensing = nullptr;
    BLE::BleService* bleService = nullptr;
    SHELLY::ShellyService* shellyService = nullptr;
    WiFiSettingsService* wifiSettingsService = nullptr;
};

bool sendSnapshotDoc(WebSocketBroadcaster* ws,
                     int fd,
                     SYSTEM::SpiRamJsonDocument& doc);
void sendShellySnapshot(const SnapshotContext& ctx);
void sendSensingSnapshot(const SnapshotContext& ctx);
void sendTelemetrySnapshot(const SnapshotContext& ctx);
void sendSystemStatusSnapshot(const SnapshotContext& ctx);
void sendBleSnapshot(const SnapshotContext& ctx);
void sendAlarmsSnapshot(const SnapshotContext& ctx);

}  // namespace SYSTEM_WS
}  // namespace API
