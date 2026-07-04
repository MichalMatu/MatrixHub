/**
 * @file TelegramCommandSections.cpp
 * @brief Shared reply sections used by multiple Telegram commands
 */

#include "TelegramCommandSections.h"

#include "../../../system/rtc/RtcConfig.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace TELEGRAM::Commands::Sections {

namespace {

void writeSectionTitle(TelegramReplyBuilder& reply, bool useHeader, const char* icon, const char* title) {
    if (useHeader) {
        reply.header(icon, title);
    } else {
        reply.section(icon, title);
    }
}

}  // namespace

ExternalIpInfo fetchExternalIp() {
    ExternalIpInfo info;
    info.wifiConnected = WiFi.isConnected();
    if (!info.wifiConnected) {
        strlcpy(info.error, "WiFi disconnected", sizeof(info.error));
        return info;
    }

    HTTPClient http;
    http.begin("http://api.ipify.org");
    http.setTimeout(3000);

    const uint32_t startCheck = millis();
    const int httpCode = http.GET();
    info.pingMs = millis() - startCheck;

    if (httpCode == HTTP_CODE_OK) {
        WiFiClient* stream = http.getStreamPtr();
        if (!stream) {
            strlcpy(info.error, "response stream missing", sizeof(info.error));
            http.end();
            return info;
        }

        size_t available = stream->available();
        const uint32_t startWait = millis();
        while (available == 0 && millis() - startWait < 1000) {
            vTaskDelay(pdMS_TO_TICKS(10));
            available = stream->available();
        }

        if (available > 0) {
            const size_t bytesRead = stream->readBytes(info.ip, sizeof(info.ip) - 1);
            info.ip[bytesRead] = '\0';
            info.success = bytesRead > 0;
            if (!info.success) {
                strlcpy(info.error, "empty response body", sizeof(info.error));
            }
        } else {
            strlcpy(info.error, "failed to read response body", sizeof(info.error));
        }
    } else {
        strlcpy(info.error, http.errorToString(httpCode).c_str(), sizeof(info.error));
    }

    http.end();
    return info;
}

void appendExternalIpSection(TelegramReplyBuilder& reply, const ExternalIpInfo& info, bool useHeader) {
    writeSectionTitle(reply, useHeader, "🌐", "External IP");

    if (!info.wifiConnected) {
        reply.detailf("WiFi disconnected.");
        return;
    }

    if (info.success) {
        reply.kvf("🔗", "Address", "https://%s", info.ip);
        reply.kvf("⏱️", "Ping", "%u ms", info.pingMs);
        return;
    }

    reply.detailf("Unavailable: %s", info.error[0] ? info.error : "unknown error");
}

void appendHealthSection(TelegramReplyBuilder& reply, bool detailed, bool useHeader) {
    const auto& stats = RTC::runtimeStats;
    const auto& heap = RTC::heapHistory;

    uint32_t hbSuccess = 0;
    uint32_t hbFail = 0;
    for (int i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
        hbSuccess += stats.heartbeatSlots[i].successCount;
        hbFail += stats.heartbeatSlots[i].failCount;
    }

    if (detailed) {
        writeSectionTitle(reply, useHeader, "🩺", "System Health");
        reply.section("📡", "Radio & Notifications");
        reply.detailf("Telegram: %u msgs / %u cmds", stats.telegramMsgsSent, stats.telegramCmdsHandled);
        reply.detailf("Webhook: %u ok / %u err", stats.webhookSent, stats.webhookFailed);
        reply.detailf("Pushover: %u ok / %u err", stats.pushoverSent, stats.pushoverFailed);
        reply.detailf("UDP Push: %u ok / %u err", stats.udpSent, stats.udpFailed);
        reply.detailf("Alarms: %u triggers / %u notes", stats.alarmsTriggered, stats.alarmNotifications);
        reply.line();
        reply.section("🔌", "Connectivity");
        reply.detailf("Shelly: %u runs / %u err", stats.shellyCmdExecuted, stats.shellyCmdFailed);
        reply.detailf("Heartbeats: %u ok / %u err", hbSuccess, hbFail);
        reply.detailf("BLE Ads: %u seen / %u matched", stats.ble.advTotal, stats.ble.advMatchedNamePrefix);
        reply.line();
        reply.section("⚙️", "Hardware Diagnostics");
        reply.detailf("I2C reads: %u ok / %u err", stats.sensorReads, stats.sensorErrors);
        reply.detailf("Maintenance sleeps: %u (last thermal: %.1f C)", stats.hygieneSleepCount, stats.lastThermalShutdownTemp);
        reply.detailf("Lowest free heap: %u KB", heap.lowestFree / 1024);
        return;
    }

    writeSectionTitle(reply, useHeader, "🩺", "Health Summary");
    reply.detailf("Telegram: %u msgs / %u cmds", stats.telegramMsgsSent, stats.telegramCmdsHandled);
    reply.detailf("Notify: wh %u/%u, po %u/%u, udp %u/%u",
                  stats.webhookSent, stats.webhookFailed,
                  stats.pushoverSent, stats.pushoverFailed,
                  stats.udpSent, stats.udpFailed);
    reply.detailf("Alarms: %u triggers / %u notes", stats.alarmsTriggered, stats.alarmNotifications);
    reply.detailf("Heartbeats: %u ok / %u err", hbSuccess, hbFail);
    reply.detailf("I2C reads: %u ok / %u err", stats.sensorReads, stats.sensorErrors);
    reply.detailf("Lowest free heap: %u KB", heap.lowestFree / 1024);
}

void appendBleSection(TelegramReplyBuilder& reply, bool detailed, bool useHeader) {
    // Telegram commands run on the notification worker, so they should use the
    // control-plane RTC accessor and render from a stable snapshot instead of
    // reading the live config store lock-free.
    const RTC::BleData ble = RTC::copyConfigSection(&RTC::ConfigStore::ble);
    const uint8_t count = ble.sensorCount;

    writeSectionTitle(reply, useHeader, "🔵", "BLE Thermometers");

    if (count == 0) {
        reply.line("No sensors configured.");
        return;
    }

    bool hasReadings = false;
    for (uint8_t i = 0; i < count; i++) {
        if (ble.readings[i].lastSeenTime > 0) {
            hasReadings = true;
            break;
        }
    }

    if (!hasReadings) {
        reply.line("No readings yet.");
        return;
    }

    const uint32_t now = millis();
    for (uint8_t i = 0; i < count && !reply.isTruncated(); i++) {
        const auto& sensor = ble.sensors[i];
        const auto& reading = ble.readings[i];
        if (reading.lastSeenTime == 0) {
            continue;
        }

        const char* name = sensor.alias[0] != '\0' ? sensor.alias : sensor.mac;
        const uint32_t ageSec = (now - reading.lastSeenTime) / 1000;

        if (detailed) {
            reply.bulletf("%s", name);
            if (reply.isTruncated()) {
                break;
            }
            reply.detailf("🌡 %.1f C  💧 %.0f%%  🔋 %u%%", reading.temperature, reading.humidity, reading.battery);
            if (reply.isTruncated()) {
                break;
            }
            reply.detailf("📶 %ddBm  ⏱ %lus ago", reading.rssi, (unsigned long)ageSec);
            if (!reply.isTruncated()) {
                reply.line();
            }
        } else {
            reply.bulletf("%s: %.1f C, %.0f%%, %u%% batt, %ddBm, %lus ago",
                          name,
                          reading.temperature,
                          reading.humidity,
                          reading.battery,
                          reading.rssi,
                          (unsigned long)ageSec);
        }
    }
}

}  // namespace TELEGRAM::Commands::Sections
