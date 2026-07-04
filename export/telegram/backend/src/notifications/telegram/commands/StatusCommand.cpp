/**
 * @file StatusCommand.cpp
 * @brief Implementation of /status command
 */

#include "StatusCommand.h"
#include "TelegramCommandSections.h"
#include "TelegramReplyBuilder.h"

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <cstdio>

#include "../../../alarms/AlarmRulesStore.h"
#include "../../../system/memory/SystemAllocator.h"
#include "../../../system/rtc/RtcConfig.h"

namespace TELEGRAM::Commands {

bool handleStatus(CommandContext& ctx) {
    const Sections::ExternalIpInfo externalIp = Sections::fetchExternalIp();

    // Calculate uptime
    uint32_t uptimeSec = millis() / 1000;
    uint32_t days = uptimeSec / 86400;
    uint32_t hours = (uptimeSec % 86400) / 3600;
    uint32_t mins = (uptimeSec % 3600) / 60;
    uint32_t secs = uptimeSec % 60;

    // Get IP address into local buffer to avoid String allocation
    char ipStr[64] = "N/A";
    char mdnsStr[64] = "N/A";
    if (WiFi.isConnected()) {
        IPAddress ip = WiFi.localIP();
        snprintf(ipStr, sizeof(ipStr), "https://%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

        const char* hostname = WiFi.getHostname();
        if (hostname && hostname[0] != '\0') {
            snprintf(mdnsStr, sizeof(mdnsStr), "https://%s.local", hostname);
        }
    }

    // Gather retained runtime/config summaries safely
    bool sensingEnabled = false;
    bool matrixEffectEnabled = false;
    uint8_t matrixBrightness = 0;
    uint8_t activeAlarms = 0;
    // Before: /status kept a full AlarmRulesData snapshot on stack even though
    // it only needs read-only iteration over the rules.
    auto alarms = SYSTEM::MEMORY::makeUniqueInPsram<ALARMS::AlarmRulesSnapshot>();
    const bool haveAlarmSnapshot = alarms && ALARMS::RULES_CONFIG::copyTo(*alarms);

    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        sensingEnabled = cfg.wifiSensing.enabled;
        matrixEffectEnabled = cfg.matrix.effectEnabled;
        matrixBrightness = cfg.matrix.brightness;
    });

    if (haveAlarmSnapshot) {
        // This snapshot is ordinary read-only config data, so PSRAM is fine.
        for (uint8_t i = 0; i < alarms->ruleCount && i < ALARMS::kMaxRules; i++) {
            if (alarms->rules[i].enabled) {
                activeAlarms++;
            }
        }
    }

    // Gather LittleFS stats
    size_t fsTotal = LittleFS.totalBytes() / 1024;
    size_t fsUsed = LittleFS.usedBytes() / 1024;
    size_t fsFree = fsTotal - fsUsed;

    // Gather PSRAM stats
    size_t psramTotal = ESP.getPsramSize() / 1024;
    size_t psramFree = ESP.getFreePsram() / 1024;

    TelegramReplyBuilder reply(ctx);
    reply.header("📊", "System Status");
    reply.section("🖥️", "System");
    reply.kvf("⏱️", "Uptime", "%ud %uh %um %us", days, hours, mins, secs);
    reply.kvf("💾", "Free heap", "%u KB", ESP.getFreeHeap() / 1024);
    reply.kvf("📦", "Max alloc", "%u KB", ESP.getMaxAllocHeap() / 1024);
    reply.kvf("🧠", "PSRAM", "%u KB free / %u KB total", psramFree, psramTotal);
    reply.kvf("📁", "LittleFS", "%u KB free / %u KB total", fsFree, fsTotal);
    reply.kvf("🌡️", "CPU temp", "%.1f C", temperatureRead());
    reply.line();
    reply.section("📶", "Connectivity");
    reply.kvf("📶", "WiFi", "%s", WiFi.isConnected() ? "Connected" : "Disconnected");
    reply.kvf("📍", "IP", "%s", ipStr);
    reply.kvf("🌐", "mDNS", "%s", mdnsStr);
    reply.kvf("📡", "RSSI", "%d dBm", WiFi.isConnected() ? WiFi.RSSI() : 0);
    reply.line();
    reply.section("⚙️", "Features");
    reply.kvf("🚶", "WiFi Sensing", "%s", sensingEnabled ? "On" : "Off");
    reply.kvf("💡", "Matrix", "%s (%u%% brightness)", matrixEffectEnabled ? "FX On" : "FX Off", matrixBrightness);
    reply.kvf("⏰", "Active alarms", "%u", activeAlarms);
    reply.line();
    Sections::appendExternalIpSection(reply, externalIp, false);
    reply.line();
    Sections::appendHealthSection(reply, false, false);
    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
