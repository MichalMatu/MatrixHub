#pragma once

#include "Arduino.h"

typedef struct esp_netif_t {
    const char* if_key = nullptr;
} esp_netif_t;

namespace TEST_STUBS::NETIF {
inline bool staAvailable = true;
inline bool apAvailable = true;
inline esp_netif_t staNetif{"WIFI_STA_DEF"};
inline esp_netif_t apNetif{"WIFI_AP_DEF"};

inline void reset() {
    staAvailable = true;
    apAvailable = true;
    staNetif.if_key = "WIFI_STA_DEF";
    apNetif.if_key = "WIFI_AP_DEF";
}
}

inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char* ifKey) {
    if (!ifKey) {
        return nullptr;
    }
    if (String(ifKey) == "WIFI_STA_DEF") {
        return TEST_STUBS::NETIF::staAvailable ? &TEST_STUBS::NETIF::staNetif : nullptr;
    }
    if (String(ifKey) == "WIFI_AP_DEF") {
        return TEST_STUBS::NETIF::apAvailable ? &TEST_STUBS::NETIF::apNetif : nullptr;
    }
    return nullptr;
}
