#include "MatrixNotificationQueue.h"
#include "../utils/ScopeLock.h"

namespace MATRIX_MANAGER {

MatrixNotificationQueue::MatrixNotificationQueue() {
    _mutex = xSemaphoreCreateMutexStatic(&_mutexBuffer);
}

bool MatrixNotificationQueue::push(const Notification& n) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) return false;

    bool overflow = false;

    if (_count >= MAX_ITEMS) {
        // Drop oldest (advance head)
        _head = (_head + 1) % MAX_ITEMS;
        _count--;
        overflow = true;
    }

    _items[_tail] = n;
    _tail = (_tail + 1) % MAX_ITEMS;
    _count++;

    return !overflow;
}

bool MatrixNotificationQueue::peek(Notification& out) const {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked() || _count == 0) return false;

    out = _items[_head];
    return true;
}

void MatrixNotificationQueue::pop() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked() || _count == 0) return;

    _head = (_head + 1) % MAX_ITEMS;
    _count--;
}

void MatrixNotificationQueue::clear() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) return;

    _head = 0;
    _tail = 0;
    _count = 0;
}

bool MatrixNotificationQueue::empty() const {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) return true;
    return _count == 0;
}

uint8_t MatrixNotificationQueue::count() const {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) return 0;
    return _count;
}

} // namespace MATRIX_MANAGER
