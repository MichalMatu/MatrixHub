#pragma once

// Mock esp_wifi types if needed
typedef struct {
    int8_t rssi;
} wifi_ap_record_t;

inline int esp_wifi_sta_get_ap_info(wifi_ap_record_t* info) {
    if (info) info->rssi = -50;
    return 0; // ESP_OK
}
