#pragma once

#include "../BaseApiService.h"
#include "../system/SystemApiFacets.h"
#include "DiagnosticsSnapshots.h"

namespace ALARMS {
class AlarmService;
class AlarmSettingsService;
}

namespace AIRMOUSE {
class AirMouseService;
}

namespace BLE {
class BleService;
class BleSettingsService;
}

namespace COMPENSATION {
class CompensationSettingsService;
}

namespace KEYBOARD {
class KeyboardService;
class KeyboardSettingsService;
}

namespace MACROS {
class MacroService;
}

namespace SYSTEM {
class HeartbeatSettingsService;
}

namespace UDPPUSH {
class UdpPusher;
class UdpSettingsService;
}

namespace USB_TERMINAL {
class UsbTerminalService;
class UsbTerminalSettingsService;
}

namespace WIFISENSING {
class WifiSensingService;
class WifiSensingSettings;
namespace CSI { class CsiService; }
}

namespace API {

struct DiagnosticsApiDeps {
    SystemApiInfoDeps info;
    SystemApiTaskDeps tasks;
    WIFISENSING::WifiSensingSettings* wifiSensingSettings = nullptr;
    WIFISENSING::WifiSensingService* wifiSensingService = nullptr;
    WIFISENSING::CSI::CsiService* csiService = nullptr;
    BLE::BleSettingsService* bleSettings = nullptr;
    BLE::BleService* bleService = nullptr;
    ALARMS::AlarmService* alarmService = nullptr;
    ALARMS::AlarmSettingsService* alarmSettings = nullptr;
    KEYBOARD::KeyboardService* keyboardService = nullptr;
    KEYBOARD::KeyboardSettingsService* keyboardSettingsService = nullptr;
    MACROS::MacroService* macroService = nullptr;
    AIRMOUSE::AirMouseService* airMouseService = nullptr;
    USB_TERMINAL::UsbTerminalService* usbTerminalService = nullptr;
    USB_TERMINAL::UsbTerminalSettingsService* usbTerminalSettings = nullptr;
    UDPPUSH::UdpPusher* udpPusher = nullptr;
    UDPPUSH::UdpSettingsService* udpSettings = nullptr;
    SYSTEM::HeartbeatSettingsService* heartbeatSettings = nullptr;
    COMPENSATION::CompensationSettingsService* compensationSettings = nullptr;
};

class DiagnosticsApiService : public BaseApiService {
public:
    DiagnosticsApiService(
        PsychicHttpServer* server,
        SecurityManager* securityManager,
        POWER::PowerManager* powerManager,
        const DiagnosticsApiDeps& deps);

    void begin() override;

private:
    DiagnosticsApiDeps _deps;

    esp_err_t handleSummary(PsychicRequest* request);
    esp_err_t handleHeap(PsychicRequest* request);
    esp_err_t handleTasks(PsychicRequest* request);
    esp_err_t handleMutexes(PsychicRequest* request);
    esp_err_t handleEndpoints(PsychicRequest* request);
    esp_err_t handleFeatures(PsychicRequest* request);

    DiagnosticsSummarySnapshot buildSummarySnapshot() const;
    DiagnosticsFeaturesSnapshot buildFeaturesSnapshot() const;
};

}  // namespace API
