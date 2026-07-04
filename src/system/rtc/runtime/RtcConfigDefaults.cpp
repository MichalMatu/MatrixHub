/**
 * @file RtcConfigDefaults.cpp
 * @brief Factory/default initialization for RTC config and retained runtime state
 */

#include "../RtcConfigInternal.h"

#include "../../../config/App.h"
#include "../../../config/System.h"
#include "../../logging/Logging.h"
#include "../RtcDefaultValues.h"
#include "../types/RtcAirMouseTypes.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "RtcConfig"

namespace RTC {

void initDefaults() {
    ConfigStore& cfg = detail::requireStore();
    memset(&cfg, 0, sizeof(ConfigStore));

    cfg.logging = LoggingData{};
    cfg.power = PowerData{};

    cfg.notification.telegramEnabled = Defaults::Notification::TelegramEnabled;
    cfg.notification.webhookEnabled = Defaults::Notification::WebhookEnabled;
    cfg.notification.pushoverEnabled = Defaults::Notification::PushoverEnabled;
    cfg.notification.commandsEnabled = Defaults::Notification::CommandsEnabled;
    cfg.notification.hasBotToken = Defaults::Notification::HasBotToken;
    cfg.notification.hasChatId = Defaults::Notification::HasChatId;
    cfg.notification.hasWebhookUrl = Defaults::Notification::HasWebhookUrl;
    cfg.notification.hasPushoverCreds = Defaults::Notification::HasPushoverCreds;

    cfg.wifiSensing = WifiSensingData{};
    cfg.ble = BleData{};

    cfg.shelly.deviceCount = 0;
    cfg.shelly.enabledCount = 0;

    cfg.alarms.ruleCount = 0;
    cfg.alarms.enabledCount = 0;

    cfg.udpPusher = UdpPusherData{};

    memset(&sensorState, 0, sizeof(sensorState));
    sensorState.magic = kSensorStateMagic;

    memset(&runtimeStats, 0, sizeof(runtimeStats));
    runtimeStats.magic = kRuntimeStatsMagic;

    memset(&heapHistory, 0, sizeof(heapHistory));

    cfg.airMouse = AirMouseData{};

    cfg.imu = ImuData{};

    cfg.matrix = MatrixData{};

    cfg.macros = MacroData{};

    cfg.keyboard = KeyboardData{};

    cfg.compensation = CompensationData{};

    cfg.usbTerminal = UsbTerminalData{};

    memset(&networkState, 0, sizeof(networkState));
    networkState.invalidate();

    markValid();
    LOGI("RTC config initialized with factory defaults");
}

}  // namespace RTC
