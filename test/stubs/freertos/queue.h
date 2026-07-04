#pragma once

#include "FreeRTOS.h"
#include <cstring>
#include <deque>
#include <vector>

typedef struct { int dummy; } StaticQueue_t;

namespace TEST_STUBS::FREERTOS {

struct QueueStub {
    UBaseType_t length = 0;
    size_t itemSize = 0;
    std::deque<std::vector<uint8_t>> items;
};

inline bool failCreateStaticQueue = false;
inline bool failNextQueueSend = false;
inline QueueHandle_t lastCreatedQueue = nullptr;
inline UBaseType_t lastQueueLength = 0;
inline size_t lastQueueItemSize = 0;
inline TickType_t lastQueueSendTimeout = 0;

inline void resetQueueStub() {
    failCreateStaticQueue = false;
    failNextQueueSend = false;
    lastCreatedQueue = nullptr;
    lastQueueLength = 0;
    lastQueueItemSize = 0;
    lastQueueSendTimeout = 0;
}

} // namespace TEST_STUBS::FREERTOS

inline QueueHandle_t xQueueCreateStatic(UBaseType_t uxQueueLength,
                                        UBaseType_t uxItemSize,
                                        uint8_t* pucQueueStorage,
                                        StaticQueue_t* pxStaticQueue) {
    (void)pucQueueStorage;
    (void)pxStaticQueue;
    if (TEST_STUBS::FREERTOS::failCreateStaticQueue) {
        return nullptr;
    }

    auto* queue = new TEST_STUBS::FREERTOS::QueueStub();
    queue->length = uxQueueLength;
    queue->itemSize = uxItemSize;
    TEST_STUBS::FREERTOS::lastCreatedQueue = queue;
    TEST_STUBS::FREERTOS::lastQueueLength = uxQueueLength;
    TEST_STUBS::FREERTOS::lastQueueItemSize = uxItemSize;
    return queue;
}

inline BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
    TEST_STUBS::FREERTOS::lastQueueSendTimeout = xTicksToWait;
    auto* queue = static_cast<TEST_STUBS::FREERTOS::QueueStub*>(xQueue);
    if (!queue || !pvItemToQueue) {
        return pdFALSE;
    }
    if (TEST_STUBS::FREERTOS::failNextQueueSend) {
        TEST_STUBS::FREERTOS::failNextQueueSend = false;
        return pdFALSE;
    }
    if (queue->items.size() >= queue->length) {
        return pdFALSE;
    }

    const auto* begin = static_cast<const uint8_t*>(pvItemToQueue);
    queue->items.emplace_back(begin, begin + queue->itemSize);
    return pdTRUE;
}

inline BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                                    const void* pvItemToQueue,
                                    BaseType_t* pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xQueueSend(xQueue, pvItemToQueue, 0);
}

inline BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    (void)xTicksToWait;
    auto* queue = static_cast<TEST_STUBS::FREERTOS::QueueStub*>(xQueue);
    if (!queue || !pvBuffer || queue->items.empty()) {
        return pdFALSE;
    }

    const auto& front = queue->items.front();
    memcpy(pvBuffer, front.data(), queue->itemSize);
    queue->items.pop_front();
    return pdTRUE;
}

inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue) {
    auto* queue = static_cast<TEST_STUBS::FREERTOS::QueueStub*>(xQueue);
    return queue ? static_cast<UBaseType_t>(queue->items.size()) : 0;
}

inline UBaseType_t uxQueueSpacesAvailable(QueueHandle_t xQueue) {
    auto* queue = static_cast<TEST_STUBS::FREERTOS::QueueStub*>(xQueue);
    if (!queue || queue->items.size() >= queue->length) {
        return 0;
    }
    return static_cast<UBaseType_t>(queue->length - queue->items.size());
}

inline void vQueueDelete(QueueHandle_t xQueue) {
    auto* queue = static_cast<TEST_STUBS::FREERTOS::QueueStub*>(xQueue);
    delete queue;
    if (TEST_STUBS::FREERTOS::lastCreatedQueue == xQueue) {
        TEST_STUBS::FREERTOS::lastCreatedQueue = nullptr;
    }
}
