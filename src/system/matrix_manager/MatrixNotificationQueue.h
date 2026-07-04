#pragma once

#include "MatrixManagerTypes.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace MATRIX_MANAGER {

/**
 * @brief Thread-safe FIFO ring buffer for Matrix notifications.
 * 
 * Fixed-capacity queue that drops the oldest item on overflow.
 *
 * Memory placement depends on the owning object. The queue itself does not
 * require PSRAM, but keeping it fixed-size avoids heap churn on the display
 * path and makes notification loss predictable under bursty producers.
 */
class MatrixNotificationQueue {
public:
    static constexpr uint8_t MAX_ITEMS = UI::MATRIX::NOTIFICATION_QUEUE_CAPACITY;

    MatrixNotificationQueue();

    /// Push a notification. Returns false if it replaced the oldest (overflow).
    bool push(const Notification& n);

    /// Peek at the front item without removing. Returns false if empty.
    bool peek(Notification& out) const;

    /// Remove the front item.
    void pop();

    /// Clear all items.
    void clear();

    bool empty() const;
    uint8_t count() const;

private:
    Notification _items[MAX_ITEMS];
    uint8_t _head = 0;
    uint8_t _tail = 0;
    uint8_t _count = 0;

    mutable SemaphoreHandle_t _mutex = nullptr;
    StaticSemaphore_t _mutexBuffer;
};

} // namespace MATRIX_MANAGER
