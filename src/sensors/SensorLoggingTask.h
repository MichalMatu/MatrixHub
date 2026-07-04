#ifndef SensorLoggingTask_h
#define SensorLoggingTask_h

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
#include <functional>

#include "model/SensorTypes.h"

namespace SENSORS { class ISensorService; }
namespace ALARMS { class AlarmService; }

class SensorLoggingTask {
public:
    static bool prepare();
    static void begin();
    static void stop();  // Graceful stop (for testing/shutdown)
    static bool isRunning();
    static void sendCommand(SensorTaskCommand cmd);
    
    // Thread-safe snapshot access
    static SensorSnapshot getSnapshot();
    static PhaseStatus getLastReadStatus();
    static PhaseStatus getLastWriteStatus();
    static SensorSnapshot getLastGoodSnapshot();
    static ErrorInfo getLastErrorInfo();

    static void setSensorService(SENSORS::ISensorService* sensorService);
    static void setAlarmService(ALARMS::AlarmService* alarmService);

    using UpdateCallback = std::function<void(const SensorSnapshot&, bool lastReadOk)>;
    static bool setUpdateCallback(UpdateCallback callback);

private:
    static void taskLoop(void* parameter);
    static bool ensureInitialized();
    static bool reapStoppedTask(TickType_t waitTicks);
    static void destroyTaskResources();

    static TaskHandle_t _taskHandle;
    static std::atomic<uint32_t> _lastReadTime_ms;
    static std::atomic<uint32_t> _lastLogTime_ms;
    static bool _initialized;
    static std::atomic<bool> _shouldRun;

    // PSRAM Stack (Static Class Members)
    static StackType_t* _taskStack;
    static StaticTask_t* _taskBuffer;
    
    static UpdateCallback _updateCallback;
    static SemaphoreHandle_t _callbackMutex;
    static SemaphoreHandle_t _stopAck;
};

#endif
