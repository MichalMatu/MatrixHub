#pragma once

#include <cstdint>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "WsTypes.h"

namespace API {
namespace WEBSOCKET {

/**
 * @brief Thread-safe PSRAM payload pool for WebSocket broadcasting
 */
class WsPayloadPool {
public:
    WsPayloadPool(const char* logTag);
    ~WsPayloadPool();

    bool init(size_t slotCount, size_t slotSize);
    void deinit();

    bool acquireSlot(size_t len, uint8_t** slotPtr, int16_t* slotIndex);
    void releaseSlot(int16_t slotIndex);
    void releaseMessageResources(const WsMessage& msg);
    
    void logDrop(const char* reason, size_t len = 0, size_t queued = 0, size_t queueLength = 0);

    size_t getSlotCount() const { return _slotCount; }
    size_t getSlotSize() const { return _slotSize; }

private:
    const char* _logTag;
    uint8_t* _storage = nullptr;
    uint8_t* _slotState = nullptr;
    SemaphoreHandle_t _lock = nullptr;
    size_t _slotSize = 0;
    size_t _slotCount = 0;

    std::atomic<uint32_t> _dropCount{0};
    std::atomic<uint32_t> _lastLogMs{0};
};

} // namespace WEBSOCKET
} // namespace API
