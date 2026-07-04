#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace NETWORK {

inline bool isWifiReadyForHttp() {
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_OFF || mode == WIFI_AP) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    const IPAddress ip = WiFi.localIP();
    return static_cast<uint32_t>(ip) != 0;
}

} // namespace NETWORK
