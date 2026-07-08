#pragma once
#include "WiFi.h"

// Mock esp_wifi types if needed
typedef struct {
    int8_t rssi;
} wifi_ap_record_t;

typedef struct {
    int8_t rssi;
} wifi_sta_info_t;

typedef struct {
    int num;
    wifi_sta_info_t sta[10];
} wifi_sta_list_t;

inline int esp_wifi_sta_get_ap_info(wifi_ap_record_t* info) {
    if (info) info->rssi = -50;
    return 0; // ESP_OK
}

inline int esp_wifi_ap_get_sta_list(wifi_sta_list_t* stations) {
    if (!stations) {
        return -1;
    }
    stations->num = TEST_STUBS::WIFI::softApStations;
    if (stations->num > 0) {
        stations->sta[0].rssi = TEST_STUBS::WIFI::apStationRssi;
    }
    return 0;
}
