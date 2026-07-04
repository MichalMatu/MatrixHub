#pragma once

#include "SystemApiDependencies.h"

class WiFiSettingsService;

namespace POWER {
class PowerManager;
}

namespace ALARMS {
class AlarmService;
}

namespace WIFISENSING {
class WifiSensingService;
}

namespace BLE {
class BleService;
}

namespace SHELLY {
class ShellyService;
}

namespace MACROS {
class MacroService;
}

namespace AIRMOUSE {
class AirMouseService;
}

namespace TELEGRAM {
class TelegramWorker;
}

namespace API {

struct SystemApiInfoDeps {
    size_t (*getFsTotalBytes)() = nullptr;
    size_t (*getFsUsedBytes)() = nullptr;
};

struct SystemApiTaskDeps {
    bool (*isWatchdogInitialized)() = nullptr;
    uint32_t (*getWatchdogTimeoutSec)() = nullptr;
};

struct SystemApiNetworkDeps {
    WiFiSettingsService* wifiSettingsService = nullptr;
    // Routed as a queued/coordinated recovery request, not a blocking action.
    bool (*requestWiFiRecovery)(const char* reason) = nullptr;
};

struct SystemApiRouteDeps {
    SystemApiInfoDeps info;
    SystemApiTaskDeps tasks;
    SystemApiNetworkDeps network;
};

struct SystemApiBroadcastDeps {
    POWER::PowerManager* powerManager = nullptr;
    ALARMS::AlarmService* alarmService = nullptr;
    WIFISENSING::WifiSensingService* wifiSensingService = nullptr;
    BLE::BleService* bleService = nullptr;
    SHELLY::ShellyService* shellyService = nullptr;
    MACROS::MacroService* macroService = nullptr;
    AIRMOUSE::AirMouseService* airMouseService = nullptr;
    TELEGRAM::TelegramWorker* telegramWorker = nullptr;
    WiFiSettingsService* wifiSettingsService = nullptr;
    SystemApiDependencies systemDeps{};
};

}  // namespace API
