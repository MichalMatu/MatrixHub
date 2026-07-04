#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_wifi_types.h>
#include <atomic>

#include "CsiTypes.h"

namespace WIFISENSING {
namespace CSI {

class CsiDataQueue {
public:
    CsiDataQueue(size_t queueSize);
    ~CsiDataQueue();

    bool pushFromIsr(const CsiPacket& packet);
    bool pop(CsiPacket& packet, TickType_t waitTicks);
    uint32_t takeDroppedPackets();
    uint32_t getDroppedPacketsTotal() const;
    size_t getDepth() const;
    size_t getCapacity() const { return _queueSize; }
    void resetStats();
    
    // Returns true if queue was created successfully
    bool begin();

private:
    size_t _queueSize;
    QueueHandle_t _queueHandle = nullptr;
    
    // Packet storage lives in PSRAM to keep DRAM pressure low. The FreeRTOS queue
    // control block stays in internal RAM because it is touched from ISR context.
    uint8_t* _queueStorageBuffer = nullptr;
    StaticQueue_t* _queueStructure = nullptr;
    std::atomic<uint32_t> _droppedPackets{0};
    std::atomic<uint32_t> _droppedPacketsTotal{0};
};

} // namespace CSI
} // namespace WIFISENSING
