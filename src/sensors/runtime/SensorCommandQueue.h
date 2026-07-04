#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../model/SensorTypes.h"

namespace SENSORS {

class SensorCommandQueue {
public:
    static bool ensureInitialized();

    static void send(SensorTaskCommand cmd);
    static bool tryReceive(SensorTaskCommand& outCmd);

private:
    static QueueHandle_t _queue;
};

}  // namespace SENSORS
