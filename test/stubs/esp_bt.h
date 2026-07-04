#pragma once

// Mock BLE power level constants from esp_bt.h
#define ESP_PWR_LVL_N12 0
#define ESP_PWR_LVL_N9  1
#define ESP_PWR_LVL_N6  2
#define ESP_PWR_LVL_N3  3
#define ESP_PWR_LVL_N0  4
#define ESP_PWR_LVL_P3  5
#define ESP_PWR_LVL_P6  6
#define ESP_PWR_LVL_P9  7

// Mock types
typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1

typedef enum {
    ESP_BT_MODE_IDLE       = 0x00,
    ESP_BT_MODE_BLE        = 0x01,
    ESP_BT_MODE_CLASSIC_BT = 0x02,
    ESP_BT_MODE_BTDM       = 0x03,
} esp_bt_mode_t;
