#include "WsPayloadPool.h"
#include "../../../system/health/network/HttpServerHealthTracker.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/utils/ScopeLock.h"
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <cstring>
#include <Arduino.h>

#undef LOG_TAG
#define LOG_TAG "WsPayloadPool"

namespace API {
namespace WEBSOCKET {

namespace {
constexpr TickType_t kPayloadAcquireLockTimeout = pdMS_TO_TICKS(10);
constexpr TickType_t kPayloadAcquireRetryDelay = pdMS_TO_TICKS(1) > 0 ? pdMS_TO_TICKS(1) : 1;
constexpr uint8_t kPayloadAcquireRetryCount = 5;
constexpr TickType_t kPayloadPoolLockTimeout = pdMS_TO_TICKS(100);
}

WsPayloadPool::WsPayloadPool(const char* logTag) : _logTag(logTag) {
    _lock = xSemaphoreCreateMutex();
}

WsPayloadPool::~WsPayloadPool() {
    deinit();
    if (_lock) {
        vSemaphoreDelete(_lock);
        _lock = nullptr;
    }
}

bool WsPayloadPool::init(size_t slotCount, size_t slotSize) {
    deinit();

    if (slotCount == 0 || slotSize == 0) return true;

    _slotCount = slotCount;
    _slotSize = slotSize;

    _storage = (uint8_t*)heap_caps_malloc(slotCount * slotSize, MALLOC_CAP_SPIRAM);
    _slotState = (uint8_t*)heap_caps_malloc(slotCount * sizeof(uint8_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!_storage || !_slotState) {
        LOGE("[%s] Failed to allocate payload pool", _logTag);
        deinit();
        return false;
    }

    memset(_slotState, 0, slotCount * sizeof(uint8_t));
    return true;
}

void WsPayloadPool::deinit() {
    SYSTEM::ScopeLock lock(_lock, kPayloadPoolLockTimeout);
    if (!lock.isLocked()) {
        LOGE("[%s] Failed to lock payload pool for deinit", _logTag);
        return;
    }

    if (_storage) {
        heap_caps_free(_storage);
        _storage = nullptr;
    }
    if (_slotState) {
        heap_caps_free(_slotState);
        _slotState = nullptr;
    }

    _slotSize = 0;
    _slotCount = 0;
    _dropCount.store(0, std::memory_order_relaxed);
    _lastLogMs.store(0, std::memory_order_relaxed);
}

bool WsPayloadPool::acquireSlot(size_t len, uint8_t** slotPtr, int16_t* slotIndex) {
    if (!slotPtr || !slotIndex || !_storage || !_slotState || !_lock) {
        return false;
    }
    if (len > _slotSize) {
        LOGW("[%s] Queue Drop: payload %zu exceeds slot size %zu", _logTag, len, _slotSize);
        return false;
    }

    bool lockBusy = false;
    bool observedPoolExhausted = false;
    for (uint8_t attempt = 0; attempt <= kPayloadAcquireRetryCount; attempt++) {
        const TickType_t lockTimeout =
            attempt == 0 ? kPayloadAcquireLockTimeout : kPayloadAcquireRetryDelay;
        {
            SYSTEM::ScopeLock lock(_lock, lockTimeout);
            if (!lock.isLocked()) {
                lockBusy = true;
            } else {
                lockBusy = false;
                observedPoolExhausted = true;
                for (size_t i = 0; i < _slotCount; i++) {
                    if (_slotState[i] == 0) {
                        _slotState[i] = 1;
                        *slotPtr = _storage + (i * _slotSize);
                        *slotIndex = static_cast<int16_t>(i);
                        return true;
                    }
                }
            }
        }

        if (attempt == kPayloadAcquireRetryCount) {
            break;
        }
        vTaskDelay(kPayloadAcquireRetryDelay);
    }

    logDrop(lockBusy && !observedPoolExhausted ? "payload pool lock busy" : "payload pool exhausted", len);
    return false;
}

void WsPayloadPool::releaseSlot(int16_t slotIndex) {
    if (slotIndex < 0 || !_slotState || !_lock) return;

    SYSTEM::ScopeLock lock(_lock, kPayloadPoolLockTimeout);
    if (!lock.isLocked()) {
        LOGW("[%s] Failed to lock payload pool while releasing slot %d", _logTag, slotIndex);
        return;
    }

    size_t index = static_cast<size_t>(slotIndex);
    if (index < _slotCount) {
        _slotState[index] = 0;
    }
}

void WsPayloadPool::releaseMessageResources(const WsMessage& msg) {
    if (msg.payloadSlot >= 0) {
        releaseSlot(msg.payloadSlot);
        return;
    }
    if (msg.isAllocated && msg.data) {
        heap_caps_free(msg.data);
    }
}

void WsPayloadPool::logDrop(const char* reason, size_t len, size_t queued, size_t queueLength) {
    SYSTEM::HEALTH::HttpServerHealthTracker::recordWsQueueDrop(len);
    const uint32_t count = _dropCount.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint32_t now = millis();
    const uint32_t lastLogMs = _lastLogMs.load(std::memory_order_relaxed);

    if (count == 1 || (now - lastLogMs) >= 5000 || (count % 32) == 0) {
        _lastLogMs.store(now, std::memory_order_relaxed);
        LOGW("[%s] Queue Drop: %s (count=%u, payload=%u, queued=%u/%u, slots=%u)",
             _logTag,
             reason ? reason : "unknown",
             count,
             static_cast<unsigned>(len),
             static_cast<unsigned>(queued),
             static_cast<unsigned>(queueLength),
             static_cast<unsigned>(_slotCount));
    }
}

} // namespace WEBSOCKET
} // namespace API
