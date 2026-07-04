#pragma once

#include <stdint.h>

typedef struct {
    int8_t rssi;
    uint32_t timestamp;
} wifi_pkt_rx_ctrl_t;

typedef struct {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t mac[6];
    int8_t* buf;
    uint16_t len;
} wifi_csi_info_t;
