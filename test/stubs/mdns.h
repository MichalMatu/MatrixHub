#pragma once

#include <vector>
#include "Arduino.h"
#include "esp_err.h"
#include "esp_netif.h"

typedef int mdns_event_actions_t;

#ifndef MDNS_EVENT_ENABLE_IP4
#define MDNS_EVENT_ENABLE_IP4 0x01
#endif
#ifndef MDNS_EVENT_ANNOUNCE_IP4
#define MDNS_EVENT_ANNOUNCE_IP4 0x02
#endif

namespace TEST_STUBS::MDNS_NATIVE {
inline esp_err_t registerResult = ESP_OK;
inline esp_err_t actionResult = ESP_OK;
inline std::vector<String> registeredIfKeys;
inline std::vector<std::pair<String, mdns_event_actions_t>> actions;

inline void reset() {
    registerResult = ESP_OK;
    actionResult = ESP_OK;
    registeredIfKeys.clear();
    actions.clear();
}
}

inline esp_err_t mdns_register_netif(esp_netif_t* netif) {
    TEST_STUBS::MDNS_NATIVE::registeredIfKeys.push_back(netif && netif->if_key ? netif->if_key : "");
    return TEST_STUBS::MDNS_NATIVE::registerResult;
}

inline esp_err_t mdns_netif_action(esp_netif_t* netif, mdns_event_actions_t actions) {
    TEST_STUBS::MDNS_NATIVE::actions.emplace_back(
        netif && netif->if_key ? netif->if_key : "",
        actions);
    return TEST_STUBS::MDNS_NATIVE::actionResult;
}
