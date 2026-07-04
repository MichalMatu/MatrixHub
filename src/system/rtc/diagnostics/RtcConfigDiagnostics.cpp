/**
 * @file RtcConfigDiagnostics.cpp
 * @brief RTC config diagnostics and retained memory status logging
 */

#include "../RtcConfigInternal.h"

#include "../../logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "RtcConfig"

namespace RTC {
namespace {

const char* yesNo(bool value) {
    return value ? "YES" : "NO";
}

const char* onOff(bool value) {
    return value ? "ON" : "OFF";
}

const char* fallbackText(const char* value, const char* fallback = "<none>") {
    return (value && value[0] != '\0') ? value : fallback;
}

const char* udpFormatToString(UdpFormat format) {
    switch (format) {
        case UdpFormat::Json:
            return "json";
        case UdpFormat::Csv:
            return "csv";
        case UdpFormat::LineProtocol:
        default:
            return "line";
    }
}

const char* clickSourceToString(ClickSource source) {
    switch (source) {
        case ClickSource::BUTTON:
            return "button";
        case ClickSource::SENSOR:
        default:
            return "sensor";
    }
}

const char* matrixAlarmModeToString(MatrixAlarmMode mode) {
    switch (mode) {
        case MatrixAlarmMode::SOLID_COLOR:
            return "solid";
        case MatrixAlarmMode::ICON:
            return "icon";
        case MatrixAlarmMode::SCROLL_TEXT:
        default:
            return "scroll";
    }
}

const char* mouseJigglerModeToString(MouseJigglerMode mode) {
    switch (mode) {
        case MouseJigglerMode::JIGGLER_STEALTH:
            return "stealth";
        case MouseJigglerMode::JIGGLER_ACTIVE:
            return "active";
        case MouseJigglerMode::JIGGLER_HUMAN:
            return "human";
        case MouseJigglerMode::JIGGLER_KEYBOARD:
            return "keyboard";
        case MouseJigglerMode::JIGGLER_OFF:
        default:
            return "off";
    }
}

}  // namespace

void logStatus() {
    const ConfigStore& cfg = detail::requireStore();
    const bool valid = isValid();

    LOGI("RTC Config Status:");
    LOGI("  Valid: %s", valid ? "YES" : "NO");
    LOGI("  Store size: %u bytes", static_cast<unsigned>(sizeof(ConfigStore)));

    const size_t trackedRuntimeBytes = getRuntimeDataSize();
    const size_t trackedBackupBytes = getBackupSlotSize();
    const size_t trackedCoreBytes = trackedBackupBytes + trackedRuntimeBytes;
    const long trackedHeadroom = static_cast<long>(kLpSramTotal) - static_cast<long>(trackedCoreBytes);

    LOGI("  RTC core: cfg=%u backup=%u runtime=%u total=%u/%u headroom=%ld",
         static_cast<unsigned>(sizeof(ConfigStore)),
         static_cast<unsigned>(trackedBackupBytes),
         static_cast<unsigned>(trackedRuntimeBytes),
         static_cast<unsigned>(trackedCoreBytes),
         static_cast<unsigned>(kLpSramTotal),
         trackedHeadroom);

    if (trackedHeadroom < 512) {
        LOGW("  RTC headroom is low (%ld bytes, tracked core only)", trackedHeadroom);
    }

    if (!valid) {
        return;
    }

    LOGI("  Version: %u", cfg.version);
    LOGI("  Logging level: %s (%d)",
         LOG::Logging::levelToString(cfg.logging.level),
         static_cast<int>(cfg.logging.level));
    LOGI("  Power: sleep=%s inact=%ums grace=%ums",
         yesNo(cfg.power.sleepEnabled),
         cfg.power.inactivityTimeoutMs,
         cfg.power.graceAfterBootMs);

    LOGI("  [NOT/TG] en=%s ready=%s token=%s chat=%s cmd=%s",
         yesNo(cfg.notification.telegramEnabled),
         yesNo(cfg.notification.isTelegramReady()),
         yesNo(cfg.notification.hasBotToken),
         yesNo(cfg.notification.hasChatId),
         yesNo(cfg.notification.commandsEnabled));
    LOGI("  [NOT/WH] en=%s ready=%s url=%s",
         yesNo(cfg.notification.webhookEnabled),
         yesNo(cfg.notification.isWebhookReady()),
         yesNo(cfg.notification.hasWebhookUrl));
    LOGI("  [NOT/PO] en=%s ready=%s creds=%s",
         yesNo(cfg.notification.pushoverEnabled),
         yesNo(cfg.notification.isPushoverReady()),
         yesNo(cfg.notification.hasPushoverCreds));

    LOGI("  [WIFI] Sensing: %s int=%ums thr=%.2f",
         yesNo(cfg.wifiSensing.enabled),
         cfg.wifiSensing.sampleIntervalMs,
         cfg.wifiSensing.varianceThreshold);
    LOGI("  [BLE] Enabled:%s sensors=%u",
         yesNo(cfg.ble.enabled),
         cfg.ble.sensorCount);
    LOGI("  [SHE] Devices: %u/%u enabled=%u",
         cfg.shelly.deviceCount,
         RTC::kMaxShellyDevices,
         cfg.shelly.enabledCount);
    LOGI("  [ALM] Rules: %u/%u enabled=%u runtime=RTC rules=PSRAM",
         cfg.alarms.ruleCount,
         RTC::kMaxAlarmRules,
         cfg.alarms.enabledCount);
    LOGI("  [UDP] Pusher: %s ready=%s host=%s port=%u fmt=%s int=%ums",
         yesNo(cfg.udpPusher.enabled),
         yesNo(cfg.udpPusher.isValid()),
         fallbackText(cfg.udpPusher.host),
         cfg.udpPusher.port,
         udpFormatToString(cfg.udpPusher.format),
         cfg.udpPusher.intervalMs);
    LOGI("  [MOU] AirMouse: mov=%s click=%s src=%s sens=%.0f,%.0f dead=%.2f jig=%s",
         onOff(cfg.airMouse.movementEnabled),
         onOff(cfg.airMouse.clickEnabled),
         clickSourceToString(cfg.airMouse.clickSource),
         cfg.airMouse.sensitivityX,
         cfg.airMouse.sensitivityY,
         cfg.airMouse.deadzone,
         mouseJigglerModeToString(cfg.airMouse.jiggler.mode));

    LOGI("  [MAT] Matrix: bri=%u rot=%u auto=%s alarm=%s fx=%s mode=%u spd=%u c1=0x%06X menu=%s",
         cfg.matrix.brightness,
         cfg.matrix.rotation,
         yesNo(cfg.matrix.autoRotate),
         matrixAlarmModeToString(cfg.matrix.alarmMode),
         onOff(cfg.matrix.effectEnabled),
         cfg.matrix.effectMode,
         cfg.matrix.effectSpeed,
         cfg.matrix.effectColor,
         yesNo(cfg.matrix.menu.enabled));

    LOGI("  [MAC] Macros: %s delay=%ums script=%s",
         yesNo(cfg.macros.enabled),
         cfg.macros.bootDelay,
         fallbackText(cfg.macros.bootScript));

    LOGI("  [KEY] Keyboard: %s",
         yesNo(cfg.keyboard.enabled));

    LOGI("  [CMP] SCD4x: %s base=%.1f ref=%.1f slope=%.2f range=[%.1f, %.1f]",
         yesNo(cfg.compensation.enabled),
         cfg.compensation.baseTempOffset,
         cfg.compensation.referenceCpuTemp,
         cfg.compensation.tempOffsetPerCpuDegree,
         cfg.compensation.minTempOffset,
         cfg.compensation.maxTempOffset);

    LOGI("  [USB] Terminal: %s tout=%ums port=%s",
         yesNo(cfg.usbTerminal.enabled),
         cfg.usbTerminal.idleTimeoutMs,
         fallbackText(cfg.usbTerminal.targetPort));

    LOGI("  CRC32: 0x%08X (Verified)", cfg.crc32);
}

}  // namespace RTC
