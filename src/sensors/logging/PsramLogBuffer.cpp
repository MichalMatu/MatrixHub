#include "PsramLogBuffer.h"
#include <esp_heap_caps.h>
#include <cstring>
#include "../../config/System.h"
#include "../../system/utils/ScopeLock.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "PsramBuf"

namespace SENSORS {

DATALOG::BinaryLogRecord* PsramLogBuffer::_buffer = nullptr;
size_t PsramLogBuffer::_capacity = 0;
size_t PsramLogBuffer::_head = 0;
size_t PsramLogBuffer::_count = 0;
SemaphoreHandle_t PsramLogBuffer::_mutex = nullptr;

bool PsramLogBuffer::begin(size_t capacityRecords) {
    if (_buffer) return true; // Already init

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        LOGE("Failed to create mutex");
        return false;
    }

    // Allocate in PSRAM
    _buffer = (DATALOG::BinaryLogRecord*)heap_caps_malloc(
        capacityRecords * sizeof(DATALOG::BinaryLogRecord), 
        MALLOC_CAP_SPIRAM
    );

    if (!_buffer) {
        LOGE("Failed to allocate %u bytes in PSRAM", capacityRecords * sizeof(DATALOG::BinaryLogRecord));
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
        return false;
    }

    _capacity = capacityRecords;
    _head = 0;
    _count = 0;
    
    LOGI("Allocated buffer for %u records in PSRAM (%u bytes)", 
         _capacity, _capacity * sizeof(DATALOG::BinaryLogRecord));
    return true;
}

void PsramLogBuffer::end() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
    if (_buffer) {
        heap_caps_free(_buffer);
        _buffer = nullptr;
    }
    _capacity = 0;
    _head = 0;
    _count = 0;
}

bool PsramLogBuffer::push(const DATALOG::BinaryLogRecord& record) {
    if (!_buffer || !_mutex) return false;

    // FIX: Use ScopeLock (RAII) to ensure thread safety
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(SENSOR::LOG_BUFFER::IO_LOCK_TIMEOUT_MS));
    if (!lock.isLocked()) {
        return false; // Failed to acquire mutex, drop log
    }

    // Circular Buffer Logic
    // _head points to the next write position.
    // _count is total items.
    
    // Simple write
    _buffer[_head] = record;
    _head = (_head + 1) % _capacity;
    
    if (_count < _capacity) {
        _count++;
    }
    // If full, count stays at capacity (oldest overwritten)

    return true; // Mutex released automatically by ScopeLock destructor
}



bool PsramLogBuffer::getRecord(size_t index, DATALOG::BinaryLogRecord& outRecord) {
    if (!_buffer || !_mutex) return false;
    // index 0 = oldest
    if (index >= _count) return false;

    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(SENSOR::LOG_BUFFER::IO_LOCK_TIMEOUT_MS));
    if (!lock.isLocked()) return false;

    // Calculate actual index in ring buffer
    // Tail is at (_head - _count + _capacity) % _capacity
    size_t tail = (_head + _capacity - _count) % _capacity;
    size_t actualIndex = (tail + index) % _capacity;
    
    outRecord = _buffer[actualIndex];
    
    return true;
}

size_t PsramLogBuffer::copyLastRecords(DATALOG::BinaryLogRecord* outBuffer, size_t maxCount) {
    if (!_buffer || !_mutex || !outBuffer || maxCount == 0) return 0;

    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(SENSOR::LOG_BUFFER::IO_LOCK_TIMEOUT_MS));
    if (!lock.isLocked()) return 0;

    const size_t available = _count;
    if (available == 0) return 0;

    const size_t take = (maxCount < available) ? maxCount : available;

    // Snapshot the newest N records in chronological order while holding the
    // mutex only long enough for at most two contiguous memcpy segments. The
    // goal here is to keep WS/history snapshots from contending with 5s sensor
    // pushes longer than needed.
    const size_t start = (_head + _capacity - take) % _capacity;
    const size_t firstSpan = (_capacity - start < take) ? (_capacity - start) : take;
    memcpy(outBuffer,
           _buffer + start,
           firstSpan * sizeof(DATALOG::BinaryLogRecord));

    const size_t secondSpan = take - firstSpan;
    if (secondSpan > 0) {
        memcpy(outBuffer + firstSpan,
               _buffer,
               secondSpan * sizeof(DATALOG::BinaryLogRecord));
    }

    return take;
}

bool PsramLogBuffer::tryCount(size_t& outCount, TickType_t timeoutTicks) {
    if (!_mutex) return false;
    SYSTEM::ScopeLock lock(_mutex, timeoutTicks);
    if (!lock.isLocked()) return false;
    outCount = _count;
    return true;
}

bool PsramLogBuffer::tryIsEmpty(bool& outIsEmpty, TickType_t timeoutTicks) {
    if (!_mutex) return false;
    SYSTEM::ScopeLock lock(_mutex, timeoutTicks);
    if (!lock.isLocked()) return false;
    outIsEmpty = (_count == 0);
    return true;
}

bool PsramLogBuffer::tryIsFull(bool& outIsFull, TickType_t timeoutTicks) {
    if (!_mutex) return false;
    SYSTEM::ScopeLock lock(_mutex, timeoutTicks);
    if (!lock.isLocked()) return false;
    outIsFull = (_count >= _capacity);
    return true;
}

size_t PsramLogBuffer::count() {
    size_t outCount = 0;
    if (!tryCount(outCount, pdMS_TO_TICKS(SENSOR::LOG_BUFFER::STATUS_LOCK_TIMEOUT_MS))) {
        LOGW("count() lock timeout");
        return 0;
    }
    return outCount;
}

bool PsramLogBuffer::isEmpty() {
    bool outIsEmpty = false;
    if (!tryIsEmpty(outIsEmpty, pdMS_TO_TICKS(SENSOR::LOG_BUFFER::STATUS_LOCK_TIMEOUT_MS))) {
        LOGW("isEmpty() lock timeout");
        return false;
    }
    return outIsEmpty;
}

bool PsramLogBuffer::isFull() {
    bool outIsFull = false;
    if (!tryIsFull(outIsFull, pdMS_TO_TICKS(SENSOR::LOG_BUFFER::STATUS_LOCK_TIMEOUT_MS))) {
        LOGW("isFull() lock timeout");
        return false;
    }
    return outIsFull;
}

} // namespace SENSORS
