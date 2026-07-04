#pragma once
#include <cstdio>
#include <cstdarg>

// Define log level enum
typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

// Mock logging macros for native tests
#define ESP_LOGE(tag, fmt, ...) printf("E [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("D [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) printf("V [%s] " fmt "\n", tag, ##__VA_ARGS__)

// Mock ESP-IDF logging functions used by Logging.cpp
inline void esp_log_level_set(const char* tag, esp_log_level_t level) {
    (void)tag;
    (void)level;
}

inline void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
    (void)level;
    (void)tag;
    va_list args;
    va_start(args, format);
    // vprintf(format, args); // Optional: print to stdout
    va_end(args);
}

inline uint32_t esp_log_timestamp() { return 0; }

using vprintf_like_t = int (*)(const char*, va_list);

inline vprintf_like_t esp_log_set_vprintf(vprintf_like_t func) {
    return func;
}
