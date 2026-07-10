#pragma once

#include <stdint.h>

typedef struct {
    int8_t rssi;
    uint8_t rate;
    uint8_t sig_mode;
    uint8_t mcs;
    uint8_t cwb;
    uint8_t smoothing;
    uint8_t not_sounding;
    uint8_t aggregation;
    uint8_t stbc;
    uint8_t fec_coding;
    uint8_t sgi;
    int8_t noise_floor;
    uint8_t ampdu_cnt;
    uint8_t channel;
    uint8_t secondary_channel;
    uint32_t timestamp;
    uint8_t ant;
    uint16_t sig_len;
    uint8_t rx_state;
} wifi_pkt_rx_ctrl_t;

typedef struct {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t mac[6];
    uint8_t dmac[6];
    bool first_word_invalid;
    int8_t* buf;
    uint16_t len;
    uint8_t* hdr;
    uint8_t* payload;
    uint16_t payload_len;
    uint16_t rx_seq;
} wifi_csi_info_t;
