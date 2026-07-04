#pragma once

#include <Arduino.h>
#include <esp_log.h>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>
#include "../../config/System.h"
#include "LogRingBuffer.h"
#include "../utils/Random.h"

namespace LOG {

struct Settings {
    esp_log_level_t level;
};

class Logging {
public:
    static void begin(const Settings &settings);
    static void setLevel(esp_log_level_t level);
    static Settings settings();

    static bool isEnabled(esp_log_level_t level);

    static void log(esp_log_level_t level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));


    template <typename Callback>
    static void forEachTail(size_t maxLines, Callback&& callback) {
        RingBuffer::forEachTail(maxLines, std::forward<Callback>(callback));
    }
    static void clearBuffer();

    static const char *levelToString(esp_log_level_t level);
    static esp_log_level_t stringToLevel(std::string_view name, esp_log_level_t fallback);
    static void redactUrlForLog(const char* url, char* out, size_t outSize);

    // Helpers for consistent formatting
    static void logStackHwm(const char* taskName, uint32_t stackSize);
    static void logStackHwm(const char* taskName, uint32_t stackSize, uint32_t minFreeBytes);
    static void logSection(const char* title);

    // Noise suppression — called from begin(), centralizes all esp_log_level_set overrides
    static void suppressNoisyModules();

private:
    static Settings _settings;
};

inline void Logging::redactUrlForLog(const char* url, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }

    out[0] = '\0';

    auto copyLiteral = [&](const char* value) {
        size_t pos = 0;
        if (!value) {
            return;
        }
        while (value[pos] != '\0' && pos + 1 < outSize) {
            out[pos] = value[pos];
            pos++;
        }
        out[pos] = '\0';
    };

    if (!url || url[0] == '\0') {
        copyLiteral("(empty)");
        return;
    }

    size_t pos = 0;
    auto appendSpan = [&](const char* start, const char* end) {
        if (!start || !end || end <= start) {
            return;
        }
        while (start < end && pos + 1 < outSize) {
            out[pos++] = *start++;
        }
        out[pos] = '\0';
    };

    auto appendLiteral = [&](const char* value) {
        if (!value) {
            return;
        }
        appendSpan(value, value + strlen(value));
    };

    const char* const urlEnd = url + strlen(url);
    const char* const schemeSep = strstr(url, "://");
    const char* authorityStart = url;

    if (schemeSep) {
        appendSpan(url, schemeSep + 3);
        authorityStart = schemeSep + 3;
    }

    const char* authorityEnd = authorityStart;
    while (authorityEnd < urlEnd && *authorityEnd != '/' && *authorityEnd != '?' && *authorityEnd != '#') {
        authorityEnd++;
    }

    const char* userInfoEnd = nullptr;
    for (const char* p = authorityStart; p < authorityEnd; ++p) {
        if (*p == '@') {
            userInfoEnd = p;
        }
    }

    if (userInfoEnd) {
        appendLiteral("[redacted]@");
        appendSpan(userInfoEnd + 1, authorityEnd);
    } else {
        appendSpan(authorityStart, authorityEnd);
    }

    if (authorityEnd < urlEnd) {
        appendLiteral("/...");
    }

    if (pos == 0) {
        copyLiteral("[redacted-url]");
    }
}

class Stopwatch {
public:
    Stopwatch()
        : _startMs((uint32_t)millis())
        , _lastSplitMs(_startMs) {}

    uint32_t elapsedMs() const {
        return (uint32_t)millis() - _startMs;
    }

    uint32_t splitMs() {
        const uint32_t now = (uint32_t)millis();
        const uint32_t diff = now - _lastSplitMs;
        _lastSplitMs = now;
        return diff;
    }

    bool isSlowMs(uint32_t thresholdMs) const {
        return elapsedMs() > thresholdMs;
    }

private:
    uint32_t _startMs;
    uint32_t _lastSplitMs;
};

class PhaseTimer {
public:
    explicit PhaseTimer(const char* scope)
        : _scope(scope) {}

    void logStep(esp_log_level_t level, const char* tag, const char* step) {
        const uint32_t stepMs = _stopwatch.splitMs();
        Logging::log(level, tag, "[%s] %s ready (%lu ms, total %lu ms)",
                     _scope,
                     step,
                     (unsigned long)stepMs,
                     (unsigned long)_stopwatch.elapsedMs());
    }

    void logDone(esp_log_level_t level, const char* tag, const char* label) {
        Logging::log(level, tag, "[%s] %s complete (%lu ms)",
                     _scope,
                     label,
                     (unsigned long)_stopwatch.elapsedMs());
    }

private:
    const char* _scope;
    Stopwatch _stopwatch;
};

}  // namespace LOG

#ifndef CORE_DEBUG_LEVEL
#define CORE_DEBUG_LEVEL ESP_LOG_VERBOSE
#endif

#ifndef LOG_TAG
#define LOG_TAG "App"
#endif

#define LOGD(fmt, ...) LOG::Logging::log(ESP_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG::Logging::log(ESP_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG::Logging::log(ESP_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG::Logging::log(ESP_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGV(fmt, ...) LOG::Logging::log(ESP_LOG_VERBOSE, LOG_TAG, fmt, ##__VA_ARGS__)

#define LOG_THROTTLED(level, intervalMs, fmt, ...)                         \
    do {                                                                   \
        static uint32_t _lastThrottledLogMs_ = _LOG_JITTER_OFFSET();       \
        static uint32_t _suppressedThrottledLogs_ = 0;                     \
        uint32_t _now_ = (uint32_t)millis();                               \
        if (_lastThrottledLogMs_ == 0 ||                                   \
            _now_ - _lastThrottledLogMs_ >= (uint32_t)(intervalMs)) {       \
            uint32_t _suppressed_ = _suppressedThrottledLogs_;             \
            _suppressedThrottledLogs_ = 0;                                 \
            _lastThrottledLogMs_ = _now_;                                  \
            if (_suppressed_ > 0) {                                        \
                LOG::Logging::log(level, LOG_TAG, fmt " (suppressed %lu repeats)", \
                                  ##__VA_ARGS__, (unsigned long)_suppressed_); \
            } else {                                                       \
                LOG::Logging::log(level, LOG_TAG, fmt, ##__VA_ARGS__);     \
            }                                                              \
        } else if (_suppressedThrottledLogs_ < 0xFFFFFFFFu) {              \
            ++_suppressedThrottledLogs_;                                   \
        }                                                                  \
    } while (0)

#define LOG_ON_CHANGE(level, value, fmt, ...)                              \
    do {                                                                   \
        const auto _logCurrentValue_ = (value);                             \
        using _LogValueType_ = std::remove_cv_t<std::remove_reference_t<decltype(_logCurrentValue_)>>; \
        static bool _logHasLastValue_ = false;                             \
        static _LogValueType_ _logLastValue_{};                            \
        if (!_logHasLastValue_ || _logLastValue_ != _logCurrentValue_) {    \
            _logLastValue_ = _logCurrentValue_;                            \
            _logHasLastValue_ = true;                                      \
            LOG::Logging::log(level, LOG_TAG, fmt, ##__VA_ARGS__);         \
        }                                                                  \
    } while (0)

#define LOGD_THROTTLED(intervalMs, fmt, ...) LOG_THROTTLED(ESP_LOG_DEBUG, intervalMs, fmt, ##__VA_ARGS__)
#define LOGI_THROTTLED(intervalMs, fmt, ...) LOG_THROTTLED(ESP_LOG_INFO, intervalMs, fmt, ##__VA_ARGS__)
#define LOGW_THROTTLED(intervalMs, fmt, ...) LOG_THROTTLED(ESP_LOG_WARN, intervalMs, fmt, ##__VA_ARGS__)
#define LOGE_THROTTLED(intervalMs, fmt, ...) LOG_THROTTLED(ESP_LOG_ERROR, intervalMs, fmt, ##__VA_ARGS__)
#define LOGV_THROTTLED(intervalMs, fmt, ...) LOG_THROTTLED(ESP_LOG_VERBOSE, intervalMs, fmt, ##__VA_ARGS__)

#define LOGD_ON_CHANGE(value, fmt, ...) LOG_ON_CHANGE(ESP_LOG_DEBUG, value, fmt, ##__VA_ARGS__)
#define LOGI_ON_CHANGE(value, fmt, ...) LOG_ON_CHANGE(ESP_LOG_INFO, value, fmt, ##__VA_ARGS__)
#define LOGW_ON_CHANGE(value, fmt, ...) LOG_ON_CHANGE(ESP_LOG_WARN, value, fmt, ##__VA_ARGS__)
#define LOGE_ON_CHANGE(value, fmt, ...) LOG_ON_CHANGE(ESP_LOG_ERROR, value, fmt, ##__VA_ARGS__)
#define LOGV_ON_CHANGE(value, fmt, ...) LOG_ON_CHANGE(ESP_LOG_VERBOSE, value, fmt, ##__VA_ARGS__)

#define LOGD_FAST(fmt, ...) LOG::Logging::log(ESP_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGI_FAST(fmt, ...) LOG::Logging::log(ESP_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW_FAST(fmt, ...) LOG::Logging::log(ESP_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE_FAST(fmt, ...) LOG::Logging::log(ESP_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)

// --- Stack monitoring macros ---
// One-shot: log stack HWM now
#define LOG_STACK() LOG::Logging::logStackHwm(LOG_TAG, 0)
#define LOG_STACK_SIZE(size) LOG::Logging::logStackHwm(LOG_TAG, size)
#define LOG_STACK_SIZE_MIN(size, minFreeBytes) LOG::Logging::logStackHwm(LOG_TAG, size, minFreeBytes)
#define LOG_STACK_BUDGET(budget) \
    LOG::Logging::logStackHwm(LOG_TAG, (budget).stackBytes, (budget).minFreeBytes)

// To prevent all periodic logs firing at the exact same millisecond and causing a cascading delay,
// we introduce a pseudo-random jitter (0-2000ms offset) initialized once per call site.
#define _LOG_JITTER_OFFSET() \
    ((UTILS::RNG::rangeU32Exclusive(2000u) + (static_cast<uint32_t>(__LINE__) * 7u)) % 2000u)

// Periodic: log stack HWM at most once per intervalMs (self-contained timer)
#define LOG_STACK_PERIODIC_MS(stackSize, intervalMs)         \
    do {                                                     \
        static uint32_t _lastStackLog_ = _LOG_JITTER_OFFSET();\
        uint32_t _now_ = (uint32_t)millis();                  \
        if (_now_ - _lastStackLog_ >= (intervalMs)) {         \
            LOG_STACK_SIZE(stackSize);                        \
            _lastStackLog_ = _now_;                            \
        }                                                     \
    } while (0)

#define LOG_STACK_BUDGET_PERIODIC_MS(budget, intervalMs)     \
    do {                                                     \
        static uint32_t _lastStackBudgetLog_ = _LOG_JITTER_OFFSET();\
        uint32_t _now_ = (uint32_t)millis();                  \
        if (_now_ - _lastStackBudgetLog_ >= (intervalMs)) {   \
            LOG_STACK_BUDGET(budget);                         \
            _lastStackBudgetLog_ = _now_;                     \
        }                                                     \
    } while (0)

// Convenience: periodic with default interval from config
#define LOG_STACK_PERIODIC(stackSize) \
    LOG_STACK_PERIODIC_MS(stackSize, TASK_MONITOR::STACK_LOG_INTERVAL_MS)
#define LOG_STACK_BUDGET_PERIODIC(budget) \
    LOG_STACK_BUDGET_PERIODIC_MS(budget, TASK_MONITOR::STACK_LOG_INTERVAL_MS)

#define LOG_SECTION(title) LOG::Logging::logSection(title)

#if DIAG_ENABLE_BOOT_TIMING
#define LOG_PHASE_STEP(timer, label) \
    (timer).logStep(ESP_LOG_INFO, LOG_TAG, label)

#define LOG_PHASE_DONE(timer, label) \
    (timer).logDone(ESP_LOG_INFO, LOG_TAG, label)
#else
#define LOG_PHASE_STEP(timer, label) do { (void)(timer); (void)(label); } while (0)
#define LOG_PHASE_DONE(timer, label) do { (void)(timer); (void)(label); } while (0)
#endif

#define _LOG_PROFILE_MSG(level, tag, name, type, diff) \
    LOG::Logging::log(level, tag, "%s %s %u us", name, type, diff)

#if DIAG_ENABLE_RUNTIME_TIMING
#define LOG_PROFILE_START(var) uint32_t var = (uint32_t)esp_timer_get_time()

// NOTE: We cast int64_t micros to uint32_t for stack efficiency.
// Math logic (uint32_t diff = end - start) correctly handles 32-bit wrap-around 
// as long as the measured duration is < ~71 minutes.
#define LOG_PROFILE_END(var, name) \
    _LOG_PROFILE_MSG(ESP_LOG_INFO, LOG_TAG, name, "took", (uint32_t)esp_timer_get_time() - var)

#define LOG_PROFILE_END_IF_SLOW(var, name, thresholdUs) \
    do { \
        uint32_t _diff = (uint32_t)esp_timer_get_time() - var; \
        if (_diff > (uint32_t)(thresholdUs)) _LOG_PROFILE_MSG(ESP_LOG_DEBUG, LOG_TAG, name, "SLOW took", _diff); \
    } while (0)

#define LOG_PROFILE_END_PERIODIC(var, name, intervalMs) \
    do { \
        static uint32_t _lastProfileLog_ = _LOG_JITTER_OFFSET(); \
        uint32_t _now_ = (uint32_t)millis(); \
        if (_now_ - _lastProfileLog_ >= (uint32_t)(intervalMs)) { \
            _LOG_PROFILE_MSG(ESP_LOG_DEBUG, LOG_TAG, name, "periodic took", (uint32_t)esp_timer_get_time() - var); \
            _lastProfileLog_ = _now_; \
        } \
    } while (0)

#define LOG_PROFILE_END_SMART(var, name, intervalMs, thresholdUs) \
    do { \
        uint32_t _diff = (uint32_t)esp_timer_get_time() - var; \
        static uint32_t _lastProfileLog_ = _LOG_JITTER_OFFSET(); \
        static uint32_t _lastSlowLog_ = 0; \
        uint32_t _now_ = (uint32_t)millis(); \
        if (_diff > (uint32_t)(thresholdUs)) { \
            if (_now_ - _lastSlowLog_ >= TASK_MONITOR::SLOW_LOG_THROTTLE_MS) { \
                LOG::Logging::log(ESP_LOG_DEBUG, LOG_TAG, "%s SLOW took %u us", name, _diff); \
                _lastSlowLog_ = _now_; \
            } \
        } else if (_now_ - _lastProfileLog_ >= (uint32_t)(intervalMs)) { \
            LOG::Logging::log(ESP_LOG_DEBUG, LOG_TAG, "%s periodic took %u us", name, _diff); \
            _lastProfileLog_ = _now_; \
        } \
    } while (0)
#else
#define LOG_PROFILE_START(var) uint32_t var = 0u
#define LOG_PROFILE_END(var, name) do { (void)(var); (void)(name); } while (0)
#define LOG_PROFILE_END_IF_SLOW(var, name, thresholdUs) \
    do { (void)(var); (void)(name); (void)(thresholdUs); } while (0)
#define LOG_PROFILE_END_PERIODIC(var, name, intervalMs) \
    do { (void)(var); (void)(name); (void)(intervalMs); } while (0)
#define LOG_PROFILE_END_SMART(var, name, intervalMs, thresholdUs) \
    do { (void)(var); (void)(name); (void)(intervalMs); (void)(thresholdUs); } while (0)
#endif
