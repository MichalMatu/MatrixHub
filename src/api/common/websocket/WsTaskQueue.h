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

    /**
     * Keeps the queue and its payload pool owned for one complete producer
     * transaction.  The lease is acquired before a broadcaster touches pool
     * storage and is released only after enqueue/drop cleanup has finished.
     */
    class ProducerLease {
    public:
        ProducerLease() = default;
        ~ProducerLease();

        ProducerLease(const ProducerLease&) = delete;
        ProducerLease& operator=(const ProducerLease&) = delete;

        ProducerLease(ProducerLease&& other) noexcept;
        ProducerLease& operator=(ProducerLease&& other) noexcept;

        explicit operator bool() const { return _owner != nullptr; }
        bool enqueue(const WsMessage& msg);

    private:
        friend class WsTaskQueue;
        ProducerLease(WsTaskQueue* owner, QueueHandle_t queue)
            : _owner(owner), _queue(queue) {}

        void release();

        WsTaskQueue* _owner = nullptr;
        QueueHandle_t _queue = nullptr;
    };

    WsTaskQueue(const char* logTag, ProcessCallback processCb, WsPayloadPool* pool);
    ~WsTaskQueue();

    void enable(size_t queueSize, uint32_t stackSize);
    // Fence new messages and ask the worker to stop without waiting for it.
    // Terminal system transitions use this from the HTTP server task, where a
    // synchronous drain could deadlock with an in-flight httpd WebSocket send.
    // The owner must keep this queue and its payload pool alive until reset or
    // follow with disable() from a context that can safely wait.
    void requestStop();
    bool disable();

    bool isEnabled() const;
    ProducerLease acquireProducerLease();
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
    bool enqueueWithLease(QueueHandle_t queue, const WsMessage& msg);
    void releaseProducerLease();
    bool reapStoppedTask(TickType_t waitTicks);
    bool waitForEnqueueIdle(TickType_t waitTicks) const;
    void destroyQueueResources();
    
    static void broadcastTask(void* pvParameters);
};

} // namespace WEBSOCKET
} // namespace API
