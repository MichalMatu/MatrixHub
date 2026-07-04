/**
 * @file UdpPusher.cpp
 * @brief UDP data pusher implementation
 */

#include "UdpPusher.h"
#include "UdpPacketFormatter.h"
#include "../system/rtc/RtcConfig.h"
#include "../sensors/SensorLoggingTask.h"
#include "../config/System.h"
#include "../system/logging/Logging.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "UdpPush"

namespace UDPPUSH {

// Buffer size for formatted message (allocated in PSRAM)
constexpr size_t kBufferSize = 4096;

namespace {

constexpr TickType_t kStateLockTimeout = pdMS_TO_TICKS(500);
constexpr TickType_t kDestructorLockTimeout = pdMS_TO_TICKS(1000);

void freeBuffer(char*& buffer) {
    if (!buffer) {
        return;
    }
    heap_caps_free(buffer);
    buffer = nullptr;
}

bool waitForTaskSuspended(TaskHandle_t taskHandle, SemaphoreHandle_t cleanupSem, TickType_t waitTicks) {
    if (taskHandle == nullptr) {
        return true;
    }

    if (eTaskGetState(taskHandle) == eSuspended) {
        return true;
    }

    if (cleanupSem != nullptr) {
        if (xSemaphoreTake(cleanupSem, waitTicks) != pdTRUE &&
            eTaskGetState(taskHandle) != eSuspended) {
            return false;
        }
    } else if (waitTicks > 0) {
        return false;
    }

    const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    const TickType_t settleWait = pdMS_TO_TICKS(50);
    TickType_t waited = 0;

    while (eTaskGetState(taskHandle) != eSuspended && waited < settleWait) {
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return eTaskGetState(taskHandle) == eSuspended;
}

}  // namespace



namespace {
RTC::UdpPusherData loadUdpConfigSnapshot() {
    return RTC::copyConfigSection(&RTC::ConfigStore::udpPusher);
}
}  // namespace

UdpPusher::UdpPusher() {
    _stateLock = xSemaphoreCreateMutex();
}

UdpPusher::~UdpPusher() {
    if (_stateLock) {
        {
            SYSTEM::ScopeLock lock(_stateLock, kDestructorLockTimeout);
            if (lock.isLocked()) {
                if (stopWorkerLocked()) {
                    freeBuffer(_buffer);
                }
            } else {
                LOGW("Destructor could not lock UDP pusher state; deferring worker cleanup to task shutdown");
            }
        }
        vSemaphoreDelete(_stateLock);
        _stateLock = nullptr;
    }
}

void UdpPusher::begin() {
    SYSTEM::ScopeLock lock(_stateLock, kStateLockTimeout);
    if (!lock.isLocked()) {
        LOGE("Failed to lock UDP pusher state in begin()");
        return;
    }

    _startupMs = millis();
    _lastPushMs = 0;
    _initialized = false;
}

void UdpPusher::update() {
    const RTC::UdpPusherData cfg = loadUdpConfigSnapshot();
    SYSTEM::ScopeLock lock(_stateLock, kStateLockTimeout);
    if (!lock.isLocked()) {
        LOGW("UDP pusher state lock busy during update()");
        return;
    }

    (void)reapStoppedWorkerLocked(0);
    
    // Skip if disabled or not configured
    if (!cfg.isValid()) {
        if ((_initialized || _buffer || _taskHandle) && !_stopRequested.load()) {
            LOGI("Disabled - entering idle");
        }
        if (stopWorkerLocked()) {
            freeBuffer(_buffer);
        }
        _initialized = false;
        return;
    }

    uint32_t now = millis();
    
    // Wait for startup stabilization using an overflow-safe elapsed-time check.
    if (!_initialized && (now - _startupMs) < INTEGRATION::UDP::STARTUP_DELAY_MS) {
        return;
    }
    
    // Initialize on first call after startup
    if (!_initialized) {
        _lastPushMs = now;
        _initialized = true;
        LOGI("Initialized: %s:%u (format=%u, interval=%ums)", 
             cfg.host, cfg.port, (uint8_t)cfg.format, cfg.intervalMs);
        // Do first push immediately
        (void)schedulePushLocked(cfg);
        return;
    }
    
    // Check interval
    uint32_t interval = cfg.intervalMs > 0 ? cfg.intervalMs : INTEGRATION::UDP::DEFAULT_INTERVAL_MS;
    if (now - _lastPushMs < interval) {
        return;
    }
    
    _lastPushMs = now;
    (void)schedulePushLocked(cfg);
}

UdpPusher::PushNowResult UdpPusher::pushNow() {
    const RTC::UdpPusherData cfg = loadUdpConfigSnapshot();
    SYSTEM::ScopeLock lock(_stateLock, kStateLockTimeout);
    if (!lock.isLocked()) {
        LOGW("UDP pusher state lock busy during pushNow()");
        return PushNowResult::SendFailed;
    }

    if (!cfg.isValid()) {
        return PushNowResult::NotConfigured;
    }

    if (_stopRequested.load(std::memory_order_acquire) &&
        !reapStoppedWorkerLocked(0)) {
        return PushNowResult::WorkerStopping;
    }

    _lastPushMs = millis();
    _initialized = true;
    return schedulePushLocked(cfg);
}

bool UdpPusher::ensureWorkerStartedLocked() {
    if (_stopRequested.load()) {
        if (!reapStoppedWorkerLocked(0)) {
            return false;
        }
    }
    if (_taskHandle) {
        return true;
    }

    if (!_cleanupSem) {
        _cleanupSem = xSemaphoreCreateBinary();
        if (!_cleanupSem) {
            LOGE("Failed to create UDP worker cleanup semaphore");
            return false;
        }
    }

    if (!_taskStack) {
        // UDP/LwIP work should stay on an internal RAM stack.
        _taskStack = (StackType_t*)heap_caps_malloc(
            CONFIG::TASKS::STACK_UDP_PUSHER,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        _taskBuffer = (StaticTask_t*)heap_caps_malloc(
            sizeof(StaticTask_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (_taskStack && _taskBuffer) {
        _taskHandle = xTaskCreateStaticPinnedToCore(
            taskEntry,
            "UdpPusher",
            CONFIG::TASKS::STACK_UDP_PUSHER,
            this,
            CONFIG::TASKS::PRIO_UDP_PUSHER,
            _taskStack,
            _taskBuffer,
            CONFIG::TASKS::CORE_UDP_PUSHER);
    }

    if (!_taskHandle) {
        LOGE("Failed to create UDP worker task");
        if (_taskStack) {
            heap_caps_free(_taskStack);
            _taskStack = nullptr;
        }
        if (_taskBuffer) {
            heap_caps_free(_taskBuffer);
            _taskBuffer = nullptr;
        }
        if (_cleanupSem) {
            vSemaphoreDelete(_cleanupSem);
            _cleanupSem = nullptr;
        }
        return false;
    }

    LOGI("Worker started (stack=%u)", CONFIG::TASKS::STACK_UDP_PUSHER);
    return true;
}

bool UdpPusher::stopWorkerLocked() {
    if (!_taskHandle) {
        destroyWorkerResourcesLocked();
        return true;
    }

    const bool alreadyStopping = _stopRequested.exchange(true);
    if (!alreadyStopping) {
        LOGI("Stopping UDP worker task");
        _pushRequested.store(false);
        xTaskNotifyGive(_taskHandle);
    }

    if (!_cleanupSem) {
        if (!alreadyStopping) {
            LOGW("UDP worker cleanup semaphore missing; deferring stop");
        }
        return false;
    }

    const TickType_t reapWait = alreadyStopping ? 0 : pdMS_TO_TICKS(200);
    if (!reapStoppedWorkerLocked(reapWait)) {
        // Do not force-delete the worker here. It may still be inside
        // networking or logging code, and killing it mid-critical-section can
        // leave FreeRTOS mutex ownership bookkeeping inconsistent.
        if (!alreadyStopping) {
            LOGW("UDP worker stop timeout - keeping task alive until it exits cleanly");
        }
        return false;
    }

    return true;
}

bool UdpPusher::reapStoppedWorkerLocked(TickType_t waitTicks) {
    if (!_taskHandle) {
        if (_stopRequested.load()) {
            destroyWorkerResourcesLocked();
        }
        return true;
    }

    if (!_stopRequested.load()) {
        return false;
    }

    if (!_cleanupSem) {
        return false;
    }

    if (!waitForTaskSuspended(_taskHandle, _cleanupSem, waitTicks)) {
        return false;
    }

    vTaskDelete(_taskHandle);
    destroyWorkerResourcesLocked();
    return true;
}

void UdpPusher::destroyWorkerResourcesLocked() {
    _taskHandle = nullptr;
    _stopRequested.store(false);

    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }
    if (_cleanupSem) {
        vSemaphoreDelete(_cleanupSem);
        _cleanupSem = nullptr;
    }
}

UdpPusher::PushNowResult UdpPusher::schedulePushLocked(const RTC::UdpPusherData& cfg) {
    if (_stopRequested.load(std::memory_order_acquire) &&
        !reapStoppedWorkerLocked(0)) {
        return PushNowResult::WorkerStopping;
    }

    if (!ensureWorkerStartedLocked()) {
        if (_stopRequested.load(std::memory_order_acquire)) {
            return PushNowResult::WorkerStopping;
        }
        return runPushLocked(cfg);
    }

    if (!_taskHandle) {
        return runPushLocked(cfg);
    }

    _pushRequested.store(true);
    xTaskNotifyGive(_taskHandle);
    return PushNowResult::Queued;
}

void UdpPusher::taskEntry(void* param) {
    auto* self = static_cast<UdpPusher*>(param);
    if (!self) {
        vTaskDelete(nullptr);
        return;
    }
    self->taskLoop();
}

void UdpPusher::taskLoop() {
    LOG_STACK_SIZE(CONFIG::TASKS::STACK_UDP_PUSHER);

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (_stopRequested.load()) {
            break;
        }

        do {
            _pushRequested.store(false);
            SYSTEM::ScopeLock lock(_stateLock, kStateLockTimeout);
            if (!lock.isLocked()) {
                LOGW("Failed to lock UDP pusher state in worker");
                break;
            }

            if (_stopRequested.load()) {
                break;
            }

            (void)runPushLocked(loadUdpConfigSnapshot());
        } while (_pushRequested.exchange(false) && !_stopRequested.load());
    }

    if (_cleanupSem) {
        xSemaphoreGive(_cleanupSem);
    }
    vTaskSuspend(nullptr);
}

UdpPusher::PushNowResult UdpPusher::runPushLocked(const RTC::UdpPusherData& cfg) {
    if (!cfg.isValid()) {
        freeBuffer(_buffer);
        return PushNowResult::NotConfigured;
    }

    if (!_buffer) {
        _buffer = (char*)heap_caps_malloc(kBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!_buffer) {
            LOGE("Failed to allocate UDP buffer in PSRAM");
            return PushNowResult::SendFailed;
        }
        LOGD("Allocated %zu bytes in PSRAM for UDP", kBufferSize);
    }

    if (WiFi.status() != WL_CONNECTED) {
        LOGD_THROTTLED(TASK_MONITOR::INTERVAL_UDP_PUSH_MS, "Skip - WiFi not connected");
        return PushNowResult::WifiDisconnected;
    }

    LOG_PROFILE_START(udpStart);
    bool result = doSend(cfg);
    LOG_PROFILE_END_SMART(
        udpStart,
        "UDP pushNow",
        TASK_MONITOR::INTERVAL_UDP_PUSH_MS,
        TASK_MONITOR::THRESHOLD_UDP_PUSH_US);

    if (result) {
        return PushNowResult::Sent;
    }

    return PushNowResult::SendFailed;
}

bool UdpPusher::doSend(const RTC::UdpPusherData& cfg) {
    if (!cfg.isValid() || !_buffer) {
        return false;
    }

    // Get real data snapshot
    SensorSnapshot snap = SensorLoggingTask::getLastGoodSnapshot();
    
    // Format message based on configured format
    size_t len = 0;
    
    switch (cfg.format) {
        case RTC::UdpFormat::Json:
            len = UdpPacketFormatter::formatJson(_buffer, kBufferSize, snap);
            break;
        case RTC::UdpFormat::Csv:
            len = UdpPacketFormatter::formatCsv(_buffer, kBufferSize, snap);
            break;
        case RTC::UdpFormat::LineProtocol:
        default:
            len = UdpPacketFormatter::formatLineProtocol(_buffer, kBufferSize, snap);
            break;
    }
    
    if (len == 0) {
        LOGW("Format failed (or empty data)");
        return false;
    }
    
    // Send via UDP
    WiFiUDP udp;
    
    if (!udp.beginPacket(cfg.host, cfg.port)) {
        LOGW("beginPacket failed: %s:%u", cfg.host, cfg.port);
        return false;
    }
    
    udp.write((const uint8_t*)_buffer, len);
    
    if (!udp.endPacket()) {
        LOGW("endPacket failed");
        RTC::runtimeStats.udpFailed++;
        RTC::runtimeStats.udpLastSendMs = millis();
        return false;
    }
    
    RTC::runtimeStats.udpSent++;
    RTC::runtimeStats.udpLastSendMs = millis();
    LOGD_THROTTLED(TASK_MONITOR::INTERVAL_UDP_PUSH_MS, "Sent %zu bytes to %s:%u", len, cfg.host, cfg.port);
    return true;
}

}  // namespace UDPPUSH
