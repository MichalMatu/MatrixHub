#include "WsTaskQueue.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/utils/ScopeLock.h"
#include "../../../config/System.h"
#include <esp_heap_caps.h>

#undef LOG_TAG
#define LOG_TAG "WsTaskQueue"

namespace {

constexpr TickType_t kLifecycleLockTimeout = pdMS_TO_TICKS(1000);
constexpr TickType_t kEnqueueLockTimeout = pdMS_TO_TICKS(250);
constexpr TickType_t kEnqueueDrainTimeout = pdMS_TO_TICKS(500);
constexpr TickType_t kQueueSendTimeout = pdMS_TO_TICKS(10);

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

} // namespace

namespace API {
namespace WEBSOCKET {

WsTaskQueue::WsTaskQueue(const char* logTag, ProcessCallback processCb, WsPayloadPool* pool)
    : _logTag(logTag), _processCb(processCb), _pool(pool) {
    _lifecycleLock = xSemaphoreCreateMutex();
}

WsTaskQueue::ProducerLease::~ProducerLease() {
    release();
}

WsTaskQueue::ProducerLease::ProducerLease(ProducerLease&& other) noexcept
    : _owner(other._owner), _queue(other._queue) {
    other._owner = nullptr;
    other._queue = nullptr;
}

WsTaskQueue::ProducerLease& WsTaskQueue::ProducerLease::operator=(
    ProducerLease&& other) noexcept {
    if (this != &other) {
        release();
        _owner = other._owner;
        _queue = other._queue;
        other._owner = nullptr;
        other._queue = nullptr;
    }
    return *this;
}

bool WsTaskQueue::ProducerLease::enqueue(const WsMessage& msg) {
    return _owner && _queue && _owner->enqueueWithLease(_queue, msg);
}

void WsTaskQueue::ProducerLease::release() {
    if (_owner) {
        _owner->releaseProducerLease();
        _owner = nullptr;
        _queue = nullptr;
    }
}

WsTaskQueue::~WsTaskQueue() {
    if (!isBroadcastTaskContext()) {
        while (!disable()) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        LOGE("[%s] Destructor called from broadcast task context", _logTag);
    }

    if (_lifecycleLock) {
        vSemaphoreDelete(_lifecycleLock);
        _lifecycleLock = nullptr;
    }
}

bool WsTaskQueue::isBroadcastTaskContext() const {
    const TaskHandle_t handle = _broadcastTask.load(std::memory_order_acquire);
    return handle != nullptr && xTaskGetCurrentTaskHandle() == handle;
}

bool WsTaskQueue::isEnabled() const {
    return _queueAccepting.load(std::memory_order_acquire) &&
           !_shutdownRequested.load(std::memory_order_acquire) &&
           _msgQueue.load(std::memory_order_acquire) != nullptr;
}

WsTaskQueue::ProducerLease WsTaskQueue::acquireProducerLease() {
    SYSTEM::ScopeLock queueLock(_lifecycleLock, kEnqueueLockTimeout);
    if (!queueLock.isLocked()) {
        return {};
    }

    QueueHandle_t queue = _msgQueue.load(std::memory_order_acquire);
    if (!_queueAccepting.load(std::memory_order_acquire) ||
        _shutdownRequested.load(std::memory_order_acquire) ||
        !queue) {
        return {};
    }

    _enqueueInFlight.fetch_add(1, std::memory_order_acq_rel);
    return ProducerLease(this, queue);
}

bool WsTaskQueue::enqueue(const WsMessage& msg) {
    ProducerLease lease = acquireProducerLease();
    if (!lease) {
        if (_pool) {
            _pool->releaseMessageResources(msg);
            _pool->logDrop("queue disabled or lifecycle lock busy", msg.len);
        }
        return false;
    }

    return lease.enqueue(msg);
}

bool WsTaskQueue::enqueueWithLease(QueueHandle_t queue, const WsMessage& msg) {
    const bool sent = (xQueueSend(queue, &msg, kQueueSendTimeout) == pdTRUE);

    if (!sent) {
        const size_t queued = uxQueueMessagesWaiting(queue);
        const size_t length = uxQueueSpacesAvailable(queue) + queued;
        if (_pool) {
            _pool->releaseMessageResources(msg);
            _pool->logDrop("queue full", msg.len, queued, length);
        }
        return false;
    }
    return true;
}

void WsTaskQueue::releaseProducerLease() {
    // This is deliberately the producer's final queue-lifecycle operation. A
    // disable() waiter may destroy both queue and payload pool as soon as the
    // count reaches zero.
    _enqueueInFlight.fetch_sub(1, std::memory_order_acq_rel);
}

void WsTaskQueue::enable(size_t queueSize, uint32_t stackSize) {
    SYSTEM::ScopeLock queueLock(_lifecycleLock, kLifecycleLockTimeout);
    if (!queueLock.isLocked()) {
        LOGE("[%s] Failed to lock queue lifecycle for enable", _logTag);
        return;
    }

    if (_shutdownRequested.load(std::memory_order_acquire)) {
        if (!reapStoppedTask(0)) {
            LOGW("[%s] Broadcast task is still stopping; enable deferred", _logTag);
            return;
        }
    }

    if (_msgQueue.load(std::memory_order_acquire)) {
        _queueAccepting.store(true, std::memory_order_release);
        return;
    }

    _queueAccepting.store(false, std::memory_order_release);

    if (!_cleanupSem) {
        _cleanupSem = xSemaphoreCreateBinary();
        if (!_cleanupSem) {
            LOGE("[%s] Failed to create broadcast cleanup semaphore", _logTag);
            return;
        }
    }

    uint8_t* queueStorage = (uint8_t*)heap_caps_malloc(queueSize * sizeof(WsMessage), MALLOC_CAP_SPIRAM);
    StaticQueue_t* queueBuffer = (StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    if (!queueStorage || !queueBuffer) {
        LOGE("[%s] Failed to allocate queue buffers", _logTag);
        if (queueStorage) heap_caps_free(queueStorage);
        if (queueBuffer) heap_caps_free(queueBuffer);
        if (_cleanupSem) {
            vSemaphoreDelete(_cleanupSem);
            _cleanupSem = nullptr;
        }
        return;
    }

    QueueHandle_t msgQueue = xQueueCreateStatic(queueSize, sizeof(WsMessage), queueStorage, queueBuffer);
    if (!msgQueue) {
        LOGE("[%s] Failed to create static queue", _logTag);
        heap_caps_free(queueStorage);
        heap_caps_free(queueBuffer);
        if (_cleanupSem) {
            vSemaphoreDelete(_cleanupSem);
            _cleanupSem = nullptr;
        }
        return;
    }

    StackType_t* taskStack = (StackType_t*)heap_caps_malloc(stackSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    StaticTask_t* taskBuffer = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    if (!taskStack || !taskBuffer) {
        LOGE("[%s] Failed to allocate task buffers", _logTag);
        if (taskStack) heap_caps_free(taskStack);
        if (taskBuffer) heap_caps_free(taskBuffer);
        vQueueDelete(msgQueue);
        heap_caps_free(queueStorage);
        heap_caps_free(queueBuffer);
        if (_cleanupSem) {
            vSemaphoreDelete(_cleanupSem);
            _cleanupSem = nullptr;
        }
        return;
    }

    _queueStorage = queueStorage;
    _queueBuffer = queueBuffer;
    _taskStack = taskStack;
    _taskBuffer = taskBuffer;
    _msgQueue.store(msgQueue, std::memory_order_release);

    TaskHandle_t broadcastTaskHandle = xTaskCreateStaticPinnedToCore(
        broadcastTask,
        "ws_tx",
        stackSize,
        this,
        CONFIG::TASKS::PRIO_WS_BROADCAST,
        taskStack,
        taskBuffer,
        CONFIG::TASKS::CORE_WS_BROADCAST
    );

    if (!broadcastTaskHandle) {
        LOGE("[%s] Failed to create broadcast task", _logTag);
        _msgQueue.store(nullptr, std::memory_order_release);
        _queueStorage = nullptr;
        _queueBuffer = nullptr;
        _taskStack = nullptr;
        _taskBuffer = nullptr;
        heap_caps_free(taskStack);
        heap_caps_free(taskBuffer);
        vQueueDelete(msgQueue);
        heap_caps_free(queueStorage);
        heap_caps_free(queueBuffer);
        if (_cleanupSem) {
            vSemaphoreDelete(_cleanupSem);
            _cleanupSem = nullptr;
        }
        return;
    }

    _broadcastTask.store(broadcastTaskHandle, std::memory_order_release);
    _queueAccepting.store(true, std::memory_order_release);
    LOGI("[%s] Queued mode enabled statically", _logTag);
}

void WsTaskQueue::requestStop() {
    // This is intentionally lock-free and non-blocking. An enqueue that
    // already crossed the lifecycle fence owns a valid queue reference and may
    // still publish one final message; the worker will drain it before
    // suspending. Queue resources stay owned until disable() reaps the worker.
    _shutdownRequested.store(true, std::memory_order_release);
    _queueAccepting.store(false, std::memory_order_release);
}

bool WsTaskQueue::disable() {
    SYSTEM::ScopeLock queueLock(_lifecycleLock, kLifecycleLockTimeout);
    if (!queueLock.isLocked()) {
        LOGW("[%s] Failed to lock queue lifecycle for disable", _logTag);
        return false;
    }

    if (_shutdownRequested.load(std::memory_order_acquire) &&
        reapStoppedTask(0)) {
        return true;
    }

    _queueAccepting.store(false, std::memory_order_release);

    if (!waitForEnqueueIdle(kEnqueueDrainTimeout)) {
        LOGW("[%s] Timed out waiting for in-flight enqueues", _logTag);
        // Teardown has not crossed the terminal shutdown fence yet. Restore a
        // fully usable queue instead of leaving the owner in a half-disabled
        // state that a reconnect could mistake for an uninitialized queue.
        if (!_shutdownRequested.load(std::memory_order_acquire)) {
            _queueAccepting.store(true, std::memory_order_release);
        }
        return false;
    }

    TaskHandle_t broadcastTaskHandle = _broadcastTask.load(std::memory_order_acquire);

    if (isBroadcastTaskContext()) {
        LOGW("[%s] disable called from broadcast task; deferring cleanup", _logTag);
        if (!_shutdownRequested.load(std::memory_order_acquire)) {
            _queueAccepting.store(true, std::memory_order_release);
        }
        return false;
    }

    if (!broadcastTaskHandle) {
        destroyQueueResources();
        return true;
    }

    const bool alreadyStopping = _shutdownRequested.exchange(true, std::memory_order_acq_rel);
    const TickType_t reapWait = alreadyStopping ? 0 : pdMS_TO_TICKS(500);
    if (!reapStoppedTask(reapWait)) {
        if (!alreadyStopping) {
            LOGW("[%s] Broadcast task stop timeout - deferring cleanup until worker exits", _logTag);
        }
        return false;
    }

    return true;
}

bool WsTaskQueue::reapStoppedTask(TickType_t waitTicks) {
    if (_enqueueInFlight.load(std::memory_order_acquire) != 0) {
        return false;
    }

    TaskHandle_t broadcastTaskHandle = _broadcastTask.load(std::memory_order_acquire);
    if (!broadcastTaskHandle) {
        if (_shutdownRequested.load(std::memory_order_acquire) ||
            _msgQueue.load(std::memory_order_acquire) != nullptr) {
            destroyQueueResources();
        }
        return true;
    }

    if (!_shutdownRequested.load(std::memory_order_acquire) || !_cleanupSem) {
        return false;
    }

    if (!waitForTaskSuspended(broadcastTaskHandle, _cleanupSem, waitTicks)) {
        return false;
    }

    vTaskDelete(broadcastTaskHandle);
    destroyQueueResources();
    return true;
}

bool WsTaskQueue::waitForEnqueueIdle(TickType_t waitTicks) const {
    const TickType_t pollStep = pdMS_TO_TICKS(5) > 0 ? pdMS_TO_TICKS(5) : 1;
    TickType_t waited = 0;

    while (_enqueueInFlight.load(std::memory_order_acquire) != 0) {
        if (waited >= waitTicks) {
            return false;
        }
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return true;
}

void WsTaskQueue::destroyQueueResources() {
    if (_enqueueInFlight.load(std::memory_order_acquire) != 0) {
        LOGW("[%s] Refusing to destroy queue with in-flight enqueues", _logTag);
        return;
    }

    QueueHandle_t msgQueue = _msgQueue.exchange(nullptr, std::memory_order_acq_rel);
    if (msgQueue) {
        WsMessage msg;
        while (xQueueReceive(msgQueue, &msg, 0) == pdTRUE) {
            if (_pool) {
                _pool->releaseMessageResources(msg);
            }
        }
        vQueueDelete(msgQueue);
    }

    _broadcastTask.store(nullptr, std::memory_order_release);
    _queueAccepting.store(false, std::memory_order_release);
    _shutdownRequested.store(false, std::memory_order_release);

    if (_queueStorage) {
        heap_caps_free(_queueStorage);
        _queueStorage = nullptr;
    }
    if (_queueBuffer) {
        heap_caps_free(_queueBuffer);
        _queueBuffer = nullptr;
    }
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

void WsTaskQueue::broadcastTask(void* pvParameters) {
    auto* self = (WsTaskQueue*)pvParameters;
    QueueHandle_t msgQueue = self ? self->_msgQueue.load(std::memory_order_acquire) : nullptr;
    if (!self || !msgQueue) {
        LOGE("[WsTaskQueue] Task started without queue");
        vTaskDelete(NULL);
        return;
    }

    LOGI("[%s] Broadcast task started", self->_logTag);
    WsMessage msg;

    while (true) {
        if (xQueueReceive(msgQueue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (msg.isInline) {
                msg.data = msg.inlineData;
            }

            if (self->_processCb) {
                self->_processCb(msg);
            }

            if (self->_pool) {
                self->_pool->releaseMessageResources(msg);
            }

            if (self->_shutdownRequested.load(std::memory_order_acquire) &&
                self->_enqueueInFlight.load(std::memory_order_acquire) == 0 &&
                uxQueueMessagesWaiting(msgQueue) == 0) {
                break;
            }
            continue;
        }

        if (self->_shutdownRequested.load(std::memory_order_acquire) &&
            self->_enqueueInFlight.load(std::memory_order_acquire) == 0 &&
            uxQueueMessagesWaiting(msgQueue) == 0) {
            break;
        }
    }

    LOGD("[%s] Broadcast task exiting", self->_logTag);
    if (self->_cleanupSem) {
        xSemaphoreGive(self->_cleanupSem);
    }
    vTaskSuspend(nullptr);
}

} // namespace WEBSOCKET
} // namespace API
