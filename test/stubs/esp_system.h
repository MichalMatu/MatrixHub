#pragma once
#include <stdint.h>
#include <stddef.h>

// Mock ESP system types
typedef enum {
    ESP_RST_UNKNOWN,
    ESP_RST_POWERON,
    ESP_RST_EXT,
    ESP_RST_SW,
    ESP_RST_PANIC,
    ESP_RST_INT_WDT,
    ESP_RST_TASK_WDT,
    ESP_RST_WDT,
    ESP_RST_DEEPSLEEP,
    ESP_RST_BROWNOUT,
    ESP_RST_SDIO,
    ESP_RST_USB,
    ESP_RST_JTAG,
    ESP_RST_EFUSE,
    ESP_RST_PWR_GLITCH,
    ESP_RST_CPU_LOCKUP,
} esp_reset_reason_t;

namespace TEST_STUBS::ESP {
inline esp_reset_reason_t resetReason = ESP_RST_POWERON;
inline uint32_t restartCalls = 0;

inline void reset() {
    resetReason = ESP_RST_POWERON;
    restartCalls = 0;
}
}  // namespace TEST_STUBS::ESP

inline esp_reset_reason_t esp_reset_reason() {
    return TEST_STUBS::ESP::resetReason;
}

inline void esp_restart() {
    TEST_STUBS::ESP::restartCalls++;
}

inline void esp_fill_random(void* buf, size_t len) {
    if (!buf) return;
    uint8_t* out = static_cast<uint8_t*>(buf);
    static uint32_t seed = 0x12345678u;
    for (size_t i = 0; i < len; ++i) {
        seed = seed * 1664525u + 1013904223u;
        out[i] = static_cast<uint8_t>(seed >> 24);
    }
}

#ifndef ESP_MAC_WIFI_STA
#define ESP_MAC_WIFI_STA 0
#endif

inline void esp_read_mac(uint8_t* mac, int type) {
    (void)type;
    if (!mac) return;
    for (size_t i = 0; i < 6; ++i) {
        mac[i] = static_cast<uint8_t>(0xA0u + i);
    }
}

// Mock RTC_DATA_ATTR if not defined elsewhere
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif

// Mock DRAM_ATTR
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
