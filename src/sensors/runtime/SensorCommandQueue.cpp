#include "SensorCommandQueue.h"

#include "../../system/logging/Logging.h"
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "Sensor"

namespace SENSORS {

QueueHandle_t SensorCommandQueue::_queue = nullptr;
static uint8_t _queueStorage[5 * sizeof(SensorTaskCommand)]; // Static .bss memory
static StaticQueue_t _queueStruct;                           // Control block

bool SensorCommandQueue::ensureInitialized() {
    if (_queue) return true;

    // Zero DRAM allocation during runtime creation
    _queue = xQueueCreateStatic(5, sizeof(SensorTaskCommand), _queueStorage, &_queueStruct);
    if (!_queue) {
        LOGE("CMDQ: failed to create static queue");
        return false;
    }
    return true;
}

void SensorCommandQueue::send(SensorTaskCommand cmd) {
    if (!_queue) return;
    if (xQueueSend(_queue, &cmd, 0) != pdTRUE) {
        static TickType_t lastDropLogTicks = 0;
        const TickType_t nowTicks = xTaskGetTickCount();
        if ((nowTicks - lastDropLogTicks) >= pdMS_TO_TICKS(1000)) {
            LOGW("CMDQ full, drop=%u", (unsigned)cmd);
            lastDropLogTicks = nowTicks;
        }
    }
}

bool SensorCommandQueue::tryReceive(SensorTaskCommand& outCmd) {
    if (!_queue) return false;
    return xQueueReceive(_queue, &outCmd, 0) == pdTRUE;
}

}  // namespace SENSORS
