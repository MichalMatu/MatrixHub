#include "SystemStatusBroadcaster.h"
#include "SystemStatusBroadcaster.h"
#include "../../common/ChannelSubscriptions.h"
#include "../../../macros/MacroService.h"
#include "../../../ble/BleService.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

namespace API {

SystemStatusBroadcaster::SystemStatusBroadcaster() {}

SystemStatusBroadcaster::~SystemStatusBroadcaster() {
   if (_broadcastTimer) {
       xTimerDelete(_broadcastTimer, 0);
   }
}

void SystemStatusBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, BLE::BleService* bleService, MACROS::MacroService* macroService) {
    _systemWs = systemWs;
    _channels = channels;
    _bleService = bleService;

    if (macroService) {
        macroService->setUpdateCallback([this](const MACROS::MacroState& state) {
            broadcastMacroState(state);
        });
    }

    if (!_broadcastTimer) {
        _broadcastTimer = xTimerCreate("SysStatus", pdMS_TO_TICKS(1000), pdTRUE, this, onSystemTimer);
    }
}

void SystemStatusBroadcaster::startTimer() {
    if (_broadcastTimer) {
        xTimerStart(_broadcastTimer, 0);
        broadcastStatus();
    }
}

void SystemStatusBroadcaster::stopTimer() {
    if (_broadcastTimer) {
        xTimerStop(_broadcastTimer, 0);
    }
}

void SystemStatusBroadcaster::onSystemTimer(TimerHandle_t xTimer) {
    auto* self = (SystemStatusBroadcaster*)pvTimerGetTimerID(xTimer);
    if (self) {
        self->broadcastStatus();
    }
}

void SystemStatusBroadcaster::broadcastStatus() {
    if (!_systemWs || !_systemWs->hasClients()) return;

    // Packet Layout (Binary) - 10 bytes total:
    // [0]    Magic (0xA5)
    // [1-4]  Timestamp (UTC Epoch, uint32 LE)
    // [5]    WiFi Status (0=Disc, 3=Conn, uint8)
    // [6]    WiFi Flags (bit0=sta_connected, bit1=ap_mode)
    // [7]    RSSI (int8)
    // [8-9]  CPU Temp (int16 * 10 LE)
    //
    // This packet intentionally stays tiny and cheap. It is the fast heartbeat
    // backing systemStatus.data on the frontend, while the richer system_status
    // JSON snapshot is requested separately on demand.
    //
    // Important: this heartbeat is intentionally NOT filtered through
    // ChannelSubscriptions::SYSTEM_STATUS. It acts as a transport-level
    // app-shell/top-nav signal for every /ws/system client, even when the
    // current screen only owns feature-specific channel leases such as BLE,
    // telemetry or notifications. The payload is fixed and tiny (10 bytes), so
    // keeping it global avoids forcing every screen to acquire an extra lease
    // just to keep the always-visible top bar fresh.
    //
    // In other words: channel subscriptions still gate feature streams and rich
    // snapshots, but this one binary packet is a deliberate endpoint-wide
    // heartbeat for connection/liveness/UI chrome.
    // TODO: The natural place for future "push on change" rich snapshots is this
    // broadcaster: keep the binary heartbeat at 1 Hz, but add dirty/debounce
    // logic here for broadcasting full system_status only when relevant fields
    // actually change.

    time_t now;
    time(&now);
    uint32_t ts = (uint32_t)now;

    uint8_t wifiStatus = 0;
    uint8_t wifiFlags = 0;
    int8_t rssi = 0;

    const bool staConnected = (WiFi.status() == WL_CONNECTED);
    wifi_mode_t mode = WiFi.getMode();
    const bool apMode = (mode == WIFI_AP || mode == WIFI_AP_STA);

    if (staConnected) {
        wifiStatus = 3;
        rssi = (int8_t)WiFi.RSSI();
        wifiFlags |= 0x01;
    }
    if (apMode) {
        wifiFlags |= 0x02;
        if (!staConnected) {
            rssi = -50;
        }
    }

    uint8_t buffer[10];
    size_t offset = 0;

    buffer[offset++] = 0xA5;
    memcpy(&buffer[offset], &ts, 4); offset += 4;
    buffer[offset++] = wifiStatus;
    buffer[offset++] = wifiFlags;
    buffer[offset++] = (uint8_t)rssi;

    int16_t cpuTemp = (int16_t)(temperatureRead() * 10);
    memcpy(&buffer[offset], &cpuTemp, 2); offset += 2;

    // Broadcast to every connected /ws/system client on purpose. If this ever
    // becomes channel-scoped, AppShell/top-nav ownership must move into a
    // guaranteed global system_status subscription first.
    _systemWs->broadcast(buffer, offset);
}

void SystemStatusBroadcaster::broadcastMacroState(const MACROS::MacroState& state) {
    if (!_systemWs || !_systemWs->hasClients()) return;
    if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::MACROS)) return;

    const uint8_t statusId = static_cast<uint8_t>(state.status);
    const bool statusChanged = (statusId != _lastMacroStatus);
    const uint32_t now = millis();

    if (!statusChanged && statusId == static_cast<uint8_t>(MACROS::MacroStatus::IDLE)) {
        return;
    }

    constexpr uint32_t kMacroBroadcastMinMs = 500;
    if (!statusChanged && (now - _lastMacroBroadcastMs < kMacroBroadcastMinMs)) {
        return;
    }

    uint8_t buf[256];
    size_t off = 0;

    buf[off++] = 0x4D;
    buf[off++] = statusId;

    memcpy(&buf[off], &state.currentLine, 4);
    off += 4;

    uint32_t uptime = (state.status == MACROS::MacroStatus::RUNNING) ? (millis() - state.startTime) : 0;
    memcpy(&buf[off], &uptime, 4);
    off += 4;

    size_t nameLen = state.currentScript.length();
    if (nameLen > (sizeof(buf) - off - 1)) nameLen = sizeof(buf) - off - 1;
    memcpy(&buf[off], state.currentScript.c_str(), nameLen);
    off += nameLen;
    buf[off++] = '\0';

    size_t errLen = state.lastError.length();
    if (errLen > (sizeof(buf) - off - 1)) errLen = sizeof(buf) - off - 1;
    memcpy(&buf[off], state.lastError.c_str(), errLen);
    off += errLen;
    buf[off++] = '\0';

    if (_systemWs && _channels) {
        _channels->broadcast(_systemWs, _systemWs->getServerHandle(), ChannelSubscriptions::MACROS, buf, off);
    }
    _lastMacroBroadcastMs = now;
    _lastMacroStatus = statusId;
}

} // namespace API
