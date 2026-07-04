#include "ImuManager.h"
#include "ImuService.h"
#include "../../system/logging/Logging.h"
#include "../../config/System.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <utility>

#undef LOG_TAG
#define LOG_TAG "IMU"

namespace IMU {

namespace {
constexpr uint32_t kStartRetryDelayMs = 5000;
}

const char* ImuManager::consumerName(Consumer consumer) {
    switch (consumer) {
        case Consumer::AirMouseMovement: return "AirMouseMovement";
        case Consumer::AirMouseClick: return "AirMouseClick";
        case Consumer::AutoRotate: return "AutoRotate";
        case Consumer::Alarm: return "Alarm";
        case Consumer::UiMonitor: return "UiMonitor";
        case Consumer::MatrixEffects: return "MatrixEffects";
        default: return "Unknown";
    }
}

const char* ImuManager::startErrorToString(StartError error) {
    switch (error) {
        case StartError::None: return "none";
        case StartError::MissingBackend: return "missing_backend";
        case StartError::StartFailed: return "start_failed";
        case StartError::RetryPending: return "retry_pending";
        case StartError::TransitionBusy: return "transition_busy";
        default: return "unknown";
    }
}

namespace {

void appendLabel(char* out, size_t len, const char* label, bool& first) {
    if (!out || len == 0 || !label) return;
    size_t used = strlen(out);
    if (used >= len - 1) return;
    if (!first) {
        int written = snprintf(out + used, len - used, ",");
        if (written < 0) return;
        used = strlen(out);
    }
    snprintf(out + used, len - used, "%s", label);
    first = false;
}

} // namespace

void ImuManager::formatConsumers(uint32_t mask, char* out, size_t len) {
    if (!out || len == 0) return;
    out[0] = '\0';
    bool first = true;

    if (mask & (1u << static_cast<uint8_t>(Consumer::AirMouseMovement))) {
        appendLabel(out, len, consumerName(Consumer::AirMouseMovement), first);
    }
    if (mask & (1u << static_cast<uint8_t>(Consumer::AirMouseClick))) {
        appendLabel(out, len, consumerName(Consumer::AirMouseClick), first);
    }
    if (mask & (1u << static_cast<uint8_t>(Consumer::AutoRotate))) {
        appendLabel(out, len, consumerName(Consumer::AutoRotate), first);
    }
    if (mask & (1u << static_cast<uint8_t>(Consumer::Alarm))) {
        appendLabel(out, len, consumerName(Consumer::Alarm), first);
    }
    if (mask & (1u << static_cast<uint8_t>(Consumer::UiMonitor))) {
        appendLabel(out, len, consumerName(Consumer::UiMonitor), first);
    }
    if (mask & (1u << static_cast<uint8_t>(Consumer::MatrixEffects))) {
        appendLabel(out, len, consumerName(Consumer::MatrixEffects), first);
    }

    if (out[0] == '\0') {
        snprintf(out, len, "none");
    }
}

ImuManager::ImuManager(ImuService* imuService)
    : _imuService(imuService) {
    _backend.start = [this]() { return _imuService && _imuService->begin(); };
    _backend.stop = [this]() {
        if (_imuService) {
            _imuService->stop();
        }
    };
    _backend.isInitialized = [this]() {
        return _imuService && _imuService->isInitialized();
    };

    _transitionMutex = xSemaphoreCreateMutex();
    if (!_transitionMutex) {
        LOGE("IMU manager mutex alloc failed");
    }
}

ImuManager::ImuManager(Backend backend)
    : _backend(std::move(backend)) {
    _transitionMutex = xSemaphoreCreateMutex();
    if (!_transitionMutex) {
        LOGE("IMU manager mutex alloc failed");
    }
}

ImuManager::~ImuManager() {
    if (_transitionMutex) {
        vSemaphoreDelete(_transitionMutex);
        _transitionMutex = nullptr;
    }
}

bool ImuManager::backendInitialized() const {
    return _backend.isInitialized ? _backend.isInitialized() : false;
}

bool ImuManager::isRunning() const {
    return backendInitialized();
}

ManagerStatus ImuManager::getStatus() const {
    ManagerStatus status;
    status.desiredMask = _desiredMask.load(std::memory_order_acquire);
    status.runningMask = _runningMask.load(std::memory_order_acquire);
    status.initialized = backendInitialized();
    if (_transitionMutex &&
        xSemaphoreTake(_transitionMutex, pdMS_TO_TICKS(CONFIG::IMU::MANAGER_TRANSITION_TIMEOUT_MS)) == pdTRUE) {
        status.transitionInProgress = _transitionInProgress;
        status.lastStartError = _lastStartError;
        status.lastStartAttemptMs = _lastStartAttemptMs;
        status.lastStartDurationMs = _lastStartDurationMs;
        status.nextRetryMs = _nextRetryMs;
        xSemaphoreGive(_transitionMutex);
    } else {
        status.lastStartError = StartError::TransitionBusy;
    }
    return status;
}

void ImuManager::setConsumerActive(Consumer consumer, bool active) {
    const uint32_t bitMask = consumerBit(consumer);
    uint32_t prev = 0;
    uint32_t next = 0;

    if (_transitionMutex) {
        if (xSemaphoreTake(_transitionMutex, pdMS_TO_TICKS(CONFIG::IMU::MANAGER_TRANSITION_TIMEOUT_MS)) != pdTRUE) {
            LOGW("IMU transition lock timeout (consumer=%s)", consumerName(consumer));
            _lastStartError = StartError::TransitionBusy;
            return;
        }
    }

    prev = _desiredMask.load(std::memory_order_relaxed);
    next = active ? (prev | bitMask) : (prev & ~bitMask);
    if (prev == next) {
        if (_transitionMutex) xSemaphoreGive(_transitionMutex);
        return;
    }

    _desiredMask.store(next, std::memory_order_release);
    if (_transitionMutex) xSemaphoreGive(_transitionMutex);

    char desiredList[96];
    formatConsumers(next, desiredList, sizeof(desiredList));
    LOGI("IMU consumer %s -> %s (desired=[%s], mask=0x%lx)",
         consumerName(consumer),
         active ? "ON" : "OFF",
         desiredList,
         (unsigned long)next);

    reconcile();
}

void ImuManager::clearConsumers() {
    if (_transitionMutex) {
        if (xSemaphoreTake(_transitionMutex, pdMS_TO_TICKS(CONFIG::IMU::MANAGER_TRANSITION_TIMEOUT_MS)) != pdTRUE) {
            LOGW("IMU transition lock timeout during clearConsumers");
            _lastStartError = StartError::TransitionBusy;
            return;
        }
    }

    _desiredMask.store(0, std::memory_order_release);
    if (_transitionMutex) xSemaphoreGive(_transitionMutex);
    reconcile();
}

void ImuManager::tick() {
    reconcile();
}

void ImuManager::reconcile() {
    if (!_transitionMutex) {
        return;
    }

    uint32_t desired = 0;
    bool shouldStart = false;
    bool shouldStop = false;
    bool stopAfterLateStart = false;
    uint32_t now = millis();

    if (xSemaphoreTake(_transitionMutex, pdMS_TO_TICKS(CONFIG::IMU::MANAGER_TRANSITION_TIMEOUT_MS)) != pdTRUE) {
        _lastStartError = StartError::TransitionBusy;
        return;
    }

    if (_transitionInProgress) {
        xSemaphoreGive(_transitionMutex);
        return;
    }

    desired = _desiredMask.load(std::memory_order_acquire);
    const bool initialized = backendInitialized();

    if (desired == 0 && initialized) {
        _transitionInProgress = true;
        shouldStop = true;
    } else if (desired == 0) {
        _runningMask.store(0, std::memory_order_release);
        _lastStartError = StartError::None;
        _nextRetryMs = 0;
    } else if (initialized) {
        _runningMask.store(desired, std::memory_order_release);
        _lastStartError = StartError::None;
    } else if (now >= _nextRetryMs) {
        _transitionInProgress = true;
        _lastStartAttemptMs = now;
        shouldStart = true;
    } else {
        _lastStartError = StartError::RetryPending;
    }

    xSemaphoreGive(_transitionMutex);

    if (!shouldStart && !shouldStop) {
        return;
    }

    if (shouldStop) {
        char prevList[96];
        formatConsumers(_runningMask.load(std::memory_order_acquire), prevList, sizeof(prevList));
        LOGI("Stopping IMU (previous=[%s])", prevList);
        const uint32_t startMs = millis();
        if (_backend.stop) {
            _backend.stop();
        }
        const uint32_t elapsedMs = millis() - startMs;

        if (xSemaphoreTake(_transitionMutex, pdMS_TO_TICKS(CONFIG::IMU::MANAGER_TRANSITION_TIMEOUT_MS)) == pdTRUE) {
            _runningMask.store(0, std::memory_order_release);
            _lastStartDurationMs = elapsedMs;
            _lastStartError = StartError::None;
            _transitionInProgress = false;
            xSemaphoreGive(_transitionMutex);
        }
        LOGI("IMU resources released (%lu ms)", (unsigned long)elapsedMs);
        return;
    }

    if (shouldStart) {
        char desiredList[96];
        formatConsumers(desired, desiredList, sizeof(desiredList));
        LOGI("Starting IMU (reason=[%s])", desiredList);
        const uint32_t startMs = millis();
        bool started = false;
        if (_backend.start) {
            started = _backend.start();
        }
        const uint32_t elapsedMs = millis() - startMs;

        if (xSemaphoreTake(_transitionMutex, pdMS_TO_TICKS(CONFIG::IMU::MANAGER_TRANSITION_TIMEOUT_MS)) != pdTRUE) {
            if (started && _backend.stop) {
                _backend.stop();
            }
            return;
        }

        const uint32_t currentDesired = _desiredMask.load(std::memory_order_acquire);
        stopAfterLateStart = started && currentDesired == 0;
        if (currentDesired == 0) {
            _runningMask.store(0, std::memory_order_release);
            _lastStartError = StartError::None;
            _nextRetryMs = 0;
        } else if (started) {
            _runningMask.store(currentDesired, std::memory_order_release);
            _lastStartError = StartError::None;
            _nextRetryMs = 0;
        } else {
            _runningMask.store(0, std::memory_order_release);
            _lastStartError = _backend.start ? StartError::StartFailed : StartError::MissingBackend;
            _nextRetryMs = millis() + kStartRetryDelayMs;
        }
        _lastStartDurationMs = elapsedMs;
        _transitionInProgress = false;
        xSemaphoreGive(_transitionMutex);

        if (stopAfterLateStart && _backend.stop) {
            LOGI("Stopping IMU after late start - consumers cleared during startup");
            _backend.stop();
        }

        if (started && !stopAfterLateStart) {
            LOGI("IMU start ok (%lu ms)", (unsigned long)elapsedMs);
        } else if (!started) {
            LOGE("IMU start failed (reason=[%s], %lu ms), retry in %lu ms",
                 desiredList,
                 (unsigned long)elapsedMs,
                 (unsigned long)kStartRetryDelayMs);
        }
    }
}

} // namespace IMU
