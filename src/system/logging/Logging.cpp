#include "Logging.h"

#include <stdarg.h>
#include <cstring>
#include "../../config/System.h"

namespace LOG {

namespace {

const char *levelLabel(esp_log_level_t level) {
    switch (level) {
        case ESP_LOG_ERROR: return "E";
        case ESP_LOG_WARN: return "W";
        case ESP_LOG_INFO: return "I";
        case ESP_LOG_DEBUG: return "D";
        case ESP_LOG_VERBOSE: return "V";
        case ESP_LOG_NONE:
        default: return "N";
    }
}

esp_log_level_t normalizeLevel(esp_log_level_t level) {
    if (level < ESP_LOG_NONE) return ESP_LOG_NONE;
    if (level > ESP_LOG_VERBOSE) return ESP_LOG_VERBOSE;
    return level;
}

}  // namespace

Settings Logging::_settings{ESP_LOG_INFO};

void Logging::begin(const Settings &settings) {
    _settings.level = normalizeLevel(settings.level);
    RingBuffer::begin(RingBuffer::kFixedCapacity);
    
    // Align ESP-IDF global log level with configured level
    esp_log_level_set("*", _settings.level);

    // Centralized noise suppression for chatty modules
    suppressNoisyModules();

    // Print Boot Banner as the absolute first log line
    LOGI("\n\n* * * %s %s v%s * * *\n", APP::DEVICE, APP::NAME, APP::VERSION);
}

void Logging::setLevel(esp_log_level_t level) {
    _settings.level = normalizeLevel(level);
    // Apply immediately (not only on boot)
    esp_log_level_set("*", _settings.level);
    // Re-apply per-module overrides (wildcard above resets them)
    suppressNoisyModules();
}

Settings Logging::settings() {
    return _settings;
}

bool Logging::isEnabled(esp_log_level_t level) {
    return level <= _settings.level;
}

void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {
    if (!isEnabled(level)) {
        return;
    }

    // Optimize stack usage: align with RingBuffer message limit plus newline
    char message[sizeof(Line::message) + 2];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(message, sizeof(message) - 2, fmt, args);
    va_end(args);

    if (len < 0) return;

    // Ensure newline and null termination for direct ESP-IDF write
    size_t actual_len = ((size_t)len < sizeof(message) - 2) ? (size_t)len : sizeof(message) - 2;
    message[actual_len] = '\n';
    message[actual_len + 1] = '\0';

    // Output to Serial via ESP-IDF logging (one atomic write)
    esp_log_write(level, tag, "%s", message);
    
    // Remove newline for RingBuffer
    message[actual_len] = '\0';
    
    // Add to ring buffer for Live Tail
    RingBuffer::append(levelLabel(level), tag, message);
}



void Logging::clearBuffer() {
    RingBuffer::clear();
}

const char *Logging::levelToString(esp_log_level_t level) {
    switch (normalizeLevel(level)) {
        case ESP_LOG_ERROR: return "error";
        case ESP_LOG_WARN: return "warn";
        case ESP_LOG_INFO: return "info";
        case ESP_LOG_DEBUG: return "debug";
        case ESP_LOG_VERBOSE: return "verbose";
        case ESP_LOG_NONE:
        default: return "none";
    }
}

esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    const size_t len = name.size();
    if (len == 0) return fallback;
    
    char lower[16];
    if (len >= sizeof(lower)) return fallback;
    
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower(static_cast<unsigned char>(name[i]));
    }
    lower[len] = '\0';
    
    // Check length first to avoid strcmp on wrong-length strings
    switch (len) {
        case 4:
            if (memcmp(lower, "warn", 4) == 0) return ESP_LOG_WARN;
            if (memcmp(lower, "info", 4) == 0) return ESP_LOG_INFO;
            if (memcmp(lower, "none", 4) == 0) return ESP_LOG_NONE;
            break;
        case 5:
            if (memcmp(lower, "error", 5) == 0) return ESP_LOG_ERROR;
            if (memcmp(lower, "debug", 5) == 0) return ESP_LOG_DEBUG;
            break;
        case 7:
            if (memcmp(lower, "warning", 7) == 0) return ESP_LOG_WARN;
            if (memcmp(lower, "verbose", 7) == 0) return ESP_LOG_VERBOSE;
            break;
    }
    return fallback;
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    logStackHwm(taskName, stackSize, 0);
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize, uint32_t minFreeBytes) {
    uint32_t freeBytes = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    if (stackSize > 0) {
        const uint32_t used = (freeBytes >= stackSize) ? 0 : (stackSize - freeBytes);
        const uint32_t usedPct = (stackSize > 0) ? ((used * 100) / stackSize) : 0;

        if (minFreeBytes > 0 && freeBytes < minFreeBytes) {
            LOGW("[STACK] %s: %u/%u bytes used (%u%%), free=%u below min=%u",
                 taskName,
                 used,
                 stackSize,
                 usedPct,
                 freeBytes,
                 minFreeBytes);
        } else if (minFreeBytes > 0) {
            LOGD("[STACK] %s: %u/%u bytes used (%u%%), free=%u min=%u",
                 taskName,
                 used,
                 stackSize,
                 usedPct,
                 freeBytes,
                 minFreeBytes);
        } else {
            LOGD("[STACK] %s: %u/%u bytes used (%u%%)",
                 taskName,
                 used,
                 stackSize,
                 usedPct);
        }
    } else {
        LOGD("[STACK] %s: %u bytes free", taskName, freeBytes);
    }
}

void Logging::logSection(const char* title) {
    LOGI("=========== %s ===========", title);
}

void Logging::suppressNoisyModules() {
    // Pin manager (logs on every GPIO reconfiguration)
    esp_log_level_set("esp32-hal-periman", ESP_LOG_WARN);
    // HTTP server internals (noisy SSL handshake errors from client disconnects)
    esp_log_level_set("httpd", ESP_LOG_NONE);
    esp_log_level_set("esp_https_server", ESP_LOG_NONE);
    esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);
    // Wireless stack noise
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("dhcpc", ESP_LOG_WARN);
}

}  // namespace LOG
