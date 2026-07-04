#pragma once

#include <cstdint>
#include <atomic>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "WsTypes.h"
#include "WsPayloadPool.h"

namespace API {
namespace WEBSOCKET {

/**
 * @brief Manages the FreeRTOS static queue and background task for broadcasting.
 */
class WsTaskQueue {
public:
    using ProcessCallback = std::function<void(WsMessage& msg)>;

    WsTaskQueue(const char* logTag, ProcessCallback processCb, WsPayloadPool* pool);
    ~WsTaskQueue();

    void enable(size_t queueSize, uint32_t stackSize);
    bool disable();

    bool isEnabled() const;
    bool enqueue(const WsMessage& msg);

private:
    const char* _logTag;
    ProcessCallback _processCb;
    WsPayloadPool* _pool;

    std::atomic<QueueHandle_t> _msgQueue{nullptr};
    std::atomic<TaskHandle_t> _broadcastTask{nullptr};
    std::atomic<uint32_t> _enqueueInFlight{0};
    std::atomic<bool> _queueAccepting{false};
    std::atomic<bool> _shutdownRequested{false};
    SemaphoreHandle_t _lifecycleLock = nullptr;
    SemaphoreHandle_t _cleanupSem = nullptr;

    // Static buffers
    uint8_t* _queueStorage = nullptr;
    StaticQueue_t* _queueBuffer = nullptr;
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskBuffer = nullptr;

    bool isBroadcastTaskContext() const;
    bool reapStoppedTask(TickType_t waitTicks);
    bool waitForEnqueueIdle(TickType_t waitTicks) const;
    void destroyQueueResources();
    
    static void broadcastTask(void* pvParameters);
};

} // namespace WEBSOCKET
} // namespace API
