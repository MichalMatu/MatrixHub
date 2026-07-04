#include "CsiDataQueue.h"
#include <esp_heap_caps.h>
#include "../../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "CsiQueue"

namespace WIFISENSING {
namespace CSI {

CsiDataQueue::CsiDataQueue(size_t queueSize) : _queueSize(queueSize) {}

CsiDataQueue::~CsiDataQueue() {
    if (_queueHandle) {
        vQueueDelete(_queueHandle);
        _queueHandle = nullptr;
    }
    if (_queueStorageBuffer) {
        heap_caps_free(_queueStorageBuffer); // was allocated via heap_caps_malloc
        _queueStorageBuffer = nullptr;
    }
    if (_queueStructure) {
        heap_caps_free(_queueStructure);
        _queueStructure = nullptr;
    }
}

bool CsiDataQueue::begin() {
    if (_queueHandle) return true;

    size_t itemSize = sizeof(CsiPacket);
    size_t bufferSize = _queueSize * itemSize;

    // Large packet storage goes to PSRAM, but the queue metadata itself stays in
    // internal RAM so FreeRTOS ISR bookkeeping does not depend on external memory.
    _queueStorageBuffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    _queueStructure = (StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!_queueStorageBuffer || !_queueStructure) {
        LOGE("Failed to allocate PSRAM for CSI Queue");
        if (_queueStorageBuffer) {
            heap_caps_free(_queueStorageBuffer);
            _queueStorageBuffer = nullptr;
        }
        if (_queueStructure) {
            heap_caps_free(_queueStructure);
            _queueStructure = nullptr;
        }
        return false;
    }

    _queueHandle = xQueueCreateStatic(
        _queueSize,
        itemSize,
        _queueStorageBuffer,
        _queueStructure
    );

    if (!_queueHandle) {
        LOGE("Failed to create Static Queue");
        heap_caps_free(_queueStorageBuffer);
        heap_caps_free(_queueStructure);
        _queueStorageBuffer = nullptr;
        _queueStructure = nullptr;
        return false;
    }

    resetStats();
    LOGI("CSI Queue created in PSRAM (Size: %d items, Bytes: %d)", _queueSize, bufferSize);
    return true;
}

bool CsiDataQueue::pushFromIsr(const CsiPacket& packet) {
    if (!_queueHandle) return false;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Never wait in ISR context. If the queue is full we count the drop so later
    // logs can distinguish "callback throttled intentionally" from "queue overflow".
    BaseType_t res = xQueueSendFromISR(_queueHandle, &packet, &xHigherPriorityTaskWoken);
    
    if (res == pdTRUE) {
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
        return true;
    }
    _droppedPackets.fetch_add(1, std::memory_order_relaxed);
    _droppedPacketsTotal.fetch_add(1, std::memory_order_relaxed);
    return false; // Queue full
}

bool CsiDataQueue::pop(CsiPacket& packet, TickType_t waitTicks) {
    if (!_queueHandle) return false;
    return xQueueReceive(_queueHandle, &packet, waitTicks) == pdTRUE;
}

uint32_t CsiDataQueue::takeDroppedPackets() {
    return _droppedPackets.exchange(0, std::memory_order_relaxed);
}

uint32_t CsiDataQueue::getDroppedPacketsTotal() const {
    return _droppedPacketsTotal.load(std::memory_order_relaxed);
}

size_t CsiDataQueue::getDepth() const {
    if (!_queueHandle) return 0;
    return uxQueueMessagesWaiting(_queueHandle);
}

void CsiDataQueue::resetStats() {
    _droppedPackets.store(0, std::memory_order_relaxed);
    _droppedPacketsTotal.store(0, std::memory_order_relaxed);
}

} // namespace CSI
} // namespace WIFISENSING
