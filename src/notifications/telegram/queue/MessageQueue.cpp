#include "MessageQueue.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/utils/ScopeLock.h"
#include <esp_heap_caps.h>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "TgQueue"

namespace TELEGRAM {

MessageQueue::MessageQueue() {
    _mutex = xSemaphoreCreateBinary();
    if (_mutex) {
        xSemaphoreGive(_mutex);
    } else {
        LOGE("Failed to create queue mutex");
        return;
    }

    _buffer = static_cast<OutboundMessage*>(
        heap_caps_malloc(kQueueCapacity * sizeof(OutboundMessage), MALLOC_CAP_SPIRAM));
    if (!_buffer) {
        LOGE("Failed to allocate Message buffer in PSRAM");
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
        return;
    }

    LOGI("Telegram MessageQueue initialized (capacity=%zu, bytes=%u)",
         kQueueCapacity,
         static_cast<unsigned>(kQueueCapacity * sizeof(OutboundMessage)));
}

MessageQueue::~MessageQueue() {
    clear();

    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }

    if (_buffer) {
        heap_caps_free(_buffer);
        _buffer = nullptr;
    }
}

bool MessageQueue::isReady() const {
    return _mutex != nullptr && _buffer != nullptr;
}

bool MessageQueue::enqueue(const char* chatId, const char* text) {
    return enqueue(chatId, text, text ? strlen(text) : 0);
}

bool MessageQueue::enqueue(const char* chatId, const char* text, size_t textLen) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (!lock.isLocked() || !_buffer) return false;

    if (!chatId || !text || textLen == 0) return false;

    // If full, drop oldest
    if (_count >= kQueueCapacity) {
        _tail = (_tail + 1) % kQueueCapacity;
        _count--;
        LOGW("Queue full: Dropped oldest message");
    }

    OutboundMessage& msg = _buffer[_head];
    
    strncpy(msg.chatId, chatId, kMaxChatIdLen - 1);
    msg.chatId[kMaxChatIdLen - 1] = '\0';
    
    size_t copyLen = (textLen < kMaxMessageLen - 1) ? textLen : kMaxMessageLen - 1;
    memcpy(msg.text, text, copyLen);
    msg.text[copyLen] = '\0';

    _head = (_head + 1) % kQueueCapacity;
    _count++;

    LOGD("Queued: chat=%s len=%u pending=%u", chatId, (unsigned)copyLen, (unsigned)_count);
    return true;
}

bool MessageQueue::dequeue(OutboundMessage& msg) {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (!lock.isLocked() || !_buffer || _count == 0) return false;

    memcpy(&msg, &_buffer[_tail], sizeof(OutboundMessage));
    
    _tail = (_tail + 1) % kQueueCapacity;
    _count--;
    
    return true;
}

bool MessageQueue::isEmpty() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    return !lock.isLocked() || !_buffer || _count == 0;
}

size_t MessageQueue::count() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(50));
    return (lock.isLocked() && _buffer) ? _count : 0;
}

void MessageQueue::clear() {
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100));
    if (lock.isLocked() && _buffer) {
        _head = 0;
        _tail = 0;
        _count = 0;
        LOGI("Queue cleared");
    }
}

}  // namespace TELEGRAM
