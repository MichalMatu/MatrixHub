/**
 * @file LogRingBuffer.cpp
 * @brief Implementation of thread-safe ring buffer for logs
 */

#include "LogRingBuffer.h"
#include <esp_attr.h>
#include <string.h>
#include "../utils/ScopeLock.h"
#include <esp_heap_caps.h>

namespace LOG {

// Static pointers initialized to null
StaticRingBuffer<Line, RingBuffer::kFixedCapacity>* RingBuffer::_store = nullptr;
SemaphoreHandle_t RingBuffer::_mutex = nullptr;

void RingBuffer::begin(uint16_t capacity) {
    (void)capacity;
    if (!_mutex) {
        _mutex = xSemaphoreCreateBinary();
        if (!_mutex) {
            ESP_LOGE("LOG", "Failed to create LogRingBuffer semaphore.");
            return;
        }
        xSemaphoreGive(_mutex);  // binary semaphore starts "taken", release it
    }

    if (!_store) {
        // Dynamic allocation in PSRAM to keep internal DRAM available.
        void* ptr = heap_caps_malloc(sizeof(StaticRingBuffer<Line, kFixedCapacity>), MALLOC_CAP_SPIRAM);
        if (ptr) {
            _store = new (ptr) StaticRingBuffer<Line, kFixedCapacity>();
        } else {
            ESP_LOGE("LOG", "Failed to allocate %u bytes for LogRingBuffer in PSRAM. Logging disabled.",
                     (unsigned)sizeof(StaticRingBuffer<Line, kFixedCapacity>));
            return; // Allocation failed completely
        }
    }

    _store->clear();
}

void RingBuffer::end() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
    if (_store) {
        _store->~StaticRingBuffer(); // Call destructor for placement new
        heap_caps_free(_store);
        _store = nullptr;
    }
}

void RingBuffer::append(const char *levelLabel, const char *tag, const char *message) {
    if (!_mutex || !_store) {
        return;
    }

    // 1. Prepare data OUTSIDE the critical section
    // Other tasks are not blocked during string parsing and timestamp acquisition
    Line line{};
    line.timestampMs = millis();
    line.levelChar = (levelLabel && levelLabel[0]) ? levelLabel[0] : 'N';
    
    if (tag) {
        strlcpy(line.tag, tag, sizeof(line.tag));
    } else {
        line.tag[0] = '\0';
    }

    if (message) {
        strlcpy(line.message, message, sizeof(line.message));
    } else {
        line.message[0] = '\0';
    }

    // 2. BLAZING FAST CRITICAL SECTION - only for pushing to the buffer
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(LOG_CFG::RING_APPEND_LOCK_MS));
    if (lock.isLocked()) {
        _store->push(line);
    }
}

void RingBuffer::clear() {
    if (!_mutex || !_store) {
        return;
    }

    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(LOG_CFG::RING_SNAPSHOT_LOCK_MS));
    if (lock.isLocked()) {
        _store->clear();
    }
}

size_t RingBuffer::copyTail(Line* out, size_t maxLines) {
    return copyTailRange(out, maxLines, 0, maxLines);
}

size_t RingBuffer::copyTailRange(Line* out, size_t tailMaxLines, size_t offsetFromOldest, size_t maxLines) {
    if (!out || maxLines == 0 || !_mutex || !_store) {
        return 0;
    }

    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(LOG_CFG::RING_SNAPSHOT_LOCK_MS));
    if (!lock.isLocked()) {
        return 0;
    }

    const size_t count = _store->size();
    if (count == 0) {
        return 0;
    }

    const size_t tailTotal = (count < tailMaxLines) ? count : tailMaxLines;
    if (offsetFromOldest >= tailTotal) {
        return 0;
    }

    const size_t remain = tailTotal - offsetFromOldest;
    const size_t toCopy = (remain < maxLines) ? remain : maxLines;
    const size_t logicalStart = (count < kFixedCapacity) ? 0 : _store->head;
    const size_t skip = (count - tailTotal) + offsetFromOldest;
    const size_t actualStart = (logicalStart + skip) % kFixedCapacity;

    for (size_t i = 0; i < toCopy; i++) {
        const size_t idx = (actualStart + i) % kFixedCapacity;
        out[i] = _store->buffer[idx];
    }

    return toCopy;
}

}  // namespace LOG
