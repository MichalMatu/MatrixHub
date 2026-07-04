#pragma once
#include <stdint.h>
#include <functional>
#include <vector>
#include "IPAddress.h"

typedef enum {
    WIFI_POWER_19_5dBm = 78,
    WIFI_POWER_15dBm = 60,
    WIFI_POWER_11dBm = 44,
    WIFI_POWER_8_5dBm = 34
} wifi_power_t;

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA = 1,
    WIFI_STA = WIFI_MODE_STA,
    WIFI_AP = 2,
    WIFI_AP_STA = 3
} wifi_mode_t;

using WiFiMode_t = wifi_mode_t;

#ifndef WIFI_OFF
#define WIFI_OFF WIFI_MODE_NULL
#endif

typedef int wl_status_t;
static constexpr wl_status_t WL_CONNECTED = 3;
static constexpr wl_status_t WL_DISCONNECTED = 6;
static constexpr wl_status_t WL_NO_SSID_AVAIL = 1;
static constexpr wl_status_t WL_CONNECT_FAILED = 4;

typedef enum {
    ARDUINO_EVENT_WIFI_STA_CONNECTED = 0,
    ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 1,
    ARDUINO_EVENT_WIFI_STA_GOT_IP = 2,
    ARDUINO_EVENT_WIFI_AP_START = 3,
    ARDUINO_EVENT_WIFI_AP_STOP = 4
} WiFiEvent_t;

struct WiFiStaDisconnectedInfo {
    uint8_t reason = 0;
};

struct WiFiEventInfo_t {
    WiFiStaDisconnectedInfo wifi_sta_disconnected;
};

namespace TEST_STUBS::WIFI {
inline wl_status_t status = WL_CONNECTED;
inline bool connected = true;
inline uint32_t localIp = 0xC0A80001;
inline uint32_t gatewayIp = 0xC0A80001;
inline uint32_t subnetMask = 0xFFFFFF00;
inline uint32_t softApIp = 0xC0A80401;
inline const char* hostname = "device";
inline const char* ssid = "stub-ssid";
inline wifi_mode_t mode = WIFI_MODE_STA;
inline bool persistent = false;
inline bool autoReconnect = false;
inline bool sleep = false;
inline wifi_power_t txPower = WIFI_POWER_19_5dBm;
inline uint8_t channel = 1;
inline uint8_t bssid[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
inline int softApStations = 0;
inline int disconnectCalls = 0;
inline int beginCalls = 0;
inline int modeCalls = 0;
inline int softApDisconnectCalls = 0;
inline int onEventCalls = 0;
inline String lastBeginSsid;
inline String lastBeginPassword;
inline std::vector<std::pair<WiFiEvent_t, std::function<void(WiFiEvent_t, WiFiEventInfo_t)>>> eventHandlers;

inline void reset() {
    status = WL_CONNECTED;
    connected = true;
    localIp = 0xC0A80001;
    gatewayIp = 0xC0A80001;
    subnetMask = 0xFFFFFF00;
    softApIp = 0xC0A80401;
    hostname = "device";
    ssid = "stub-ssid";
    mode = WIFI_MODE_STA;
    persistent = false;
    autoReconnect = false;
    sleep = false;
    txPower = WIFI_POWER_19_5dBm;
    channel = 1;
    bssid[0] = 0x02;
    bssid[1] = 0x00;
    bssid[2] = 0x00;
    bssid[3] = 0x00;
    bssid[4] = 0x00;
    bssid[5] = 0x01;
    softApStations = 0;
    disconnectCalls = 0;
    beginCalls = 0;
    modeCalls = 0;
    softApDisconnectCalls = 0;
    onEventCalls = 0;
    lastBeginSsid = "";
    lastBeginPassword = "";
    eventHandlers.clear();
}

inline void fireEvent(WiFiEvent_t event, WiFiEventInfo_t info = {}) {
    for (const auto& [registeredEvent, handler] : eventHandlers) {
        if (registeredEvent == event && handler) {
            handler(event, info);
        }
    }
}
}

// Minimal WiFi mock
class WiFiClass {
public:
    int32_t RSSI() { return -60; }
    bool isConnected() { return TEST_STUBS::WIFI::connected; }
    wifi_power_t getTxPower() { return TEST_STUBS::WIFI::txPower; }
    bool setTxPower(wifi_power_t power) {
        TEST_STUBS::WIFI::txPower = power;
        return true;
    }
    wifi_mode_t getMode() { return TEST_STUBS::WIFI::mode; }
    bool mode(wifi_mode_t modeValue) {
        TEST_STUBS::WIFI::modeCalls++;
        TEST_STUBS::WIFI::mode = modeValue;
        return true;
    }
    template <typename Handler>
    void onEvent(Handler&& handler, WiFiEvent_t event) {
        TEST_STUBS::WIFI::onEventCalls++;
        TEST_STUBS::WIFI::eventHandlers.emplace_back(
            event,
            std::function<void(WiFiEvent_t, WiFiEventInfo_t)>(std::forward<Handler>(handler)));
    }
    void persistent(bool enabled) { TEST_STUBS::WIFI::persistent = enabled; }
    void setAutoReconnect(bool enabled) { TEST_STUBS::WIFI::autoReconnect = enabled; }
    wl_status_t status() { return TEST_STUBS::WIFI::status; }
    IPAddress localIP() { return IPAddress(TEST_STUBS::WIFI::localIp); }
    IPAddress gatewayIP() { return IPAddress(TEST_STUBS::WIFI::gatewayIp); }
    IPAddress subnetMask() { return IPAddress(TEST_STUBS::WIFI::subnetMask); }
    IPAddress softAPIP() { return IPAddress(TEST_STUBS::WIFI::softApIp); }
    int softAPgetStationNum() { return TEST_STUBS::WIFI::softApStations; }
    const char* getHostname() { return TEST_STUBS::WIFI::hostname; }
    String SSID() { return String(TEST_STUBS::WIFI::ssid); }
    const uint8_t* BSSID() { return TEST_STUBS::WIFI::bssid; }
    uint8_t channel() { return TEST_STUBS::WIFI::channel; }
    void disconnect(bool wifioff = false) {
        (void)wifioff;
        TEST_STUBS::WIFI::disconnectCalls++;
        TEST_STUBS::WIFI::connected = false;
        TEST_STUBS::WIFI::status = WL_DISCONNECTED;
    }
    bool softAPConfig(IPAddress local, IPAddress gateway, IPAddress subnet) {
        TEST_STUBS::WIFI::softApIp = static_cast<uint32_t>(local);
        TEST_STUBS::WIFI::gatewayIp = static_cast<uint32_t>(gateway);
        TEST_STUBS::WIFI::subnetMask = static_cast<uint32_t>(subnet);
        return true;
    }
    bool softAP(const char* ssid, const char* password, uint8_t channel, bool hidden, uint8_t maxClients) {
        (void)ssid;
        (void)password;
        (void)hidden;
        (void)maxClients;
        TEST_STUBS::WIFI::mode = (TEST_STUBS::WIFI::mode == WIFI_MODE_STA) ? WIFI_AP_STA : TEST_STUBS::WIFI::mode;
        TEST_STUBS::WIFI::channel = channel;
        return true;
    }
    bool softAPdisconnect(bool wifioff = false) {
        (void)wifioff;
        TEST_STUBS::WIFI::softApDisconnectCalls++;
        if (TEST_STUBS::WIFI::mode == WIFI_AP_STA) {
            TEST_STUBS::WIFI::mode = WIFI_MODE_STA;
        } else if (TEST_STUBS::WIFI::mode == WIFI_AP) {
            TEST_STUBS::WIFI::mode = WIFI_MODE_NULL;
        }
        return true;
    }
    bool config(uint32_t local, uint32_t gateway, uint32_t subnet) {
        TEST_STUBS::WIFI::localIp = local;
        TEST_STUBS::WIFI::gatewayIp = gateway;
        TEST_STUBS::WIFI::subnetMask = subnet;
        return true;
    }
    bool config(IPAddress local, IPAddress gateway, IPAddress subnet, IPAddress dns1 = INADDR_NONE, IPAddress dns2 = INADDR_NONE) {
        (void)dns1;
        (void)dns2;
        TEST_STUBS::WIFI::localIp = static_cast<uint32_t>(local);
        TEST_STUBS::WIFI::gatewayIp = static_cast<uint32_t>(gateway);
        TEST_STUBS::WIFI::subnetMask = static_cast<uint32_t>(subnet);
        return true;
    }
    bool setHostname(const char* hostname) {
        TEST_STUBS::WIFI::hostname = hostname ? hostname : "";
        return true;
    }
    void begin(const char* ssid, const char* password) {
        TEST_STUBS::WIFI::beginCalls++;
        TEST_STUBS::WIFI::lastBeginSsid = ssid ? ssid : "";
        TEST_STUBS::WIFI::lastBeginPassword = password ? password : "";
    }
    void begin(const char* ssid, const char* password, uint8_t channel, const uint8_t* bssid, bool connect) {
        (void)connect;
        begin(ssid, password);
        TEST_STUBS::WIFI::channel = channel;
        if (bssid) {
            for (size_t i = 0; i < 6; ++i) {
                TEST_STUBS::WIFI::bssid[i] = bssid[i];
            }
        }
    }
    bool setSleep(bool enabled) {
        TEST_STUBS::WIFI::sleep = enabled;
        return true;
    }
};

extern WiFiClass WiFi;
