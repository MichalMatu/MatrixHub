/**
 * @file UdpPusher.h
 * @brief UDP data pusher for LAN servers (InfluxDB, Telegraf, custom)
 * 
 * Sends sensor data via UDP to a configurable host:port.
 * Supports multiple formats: InfluxDB line protocol, JSON, CSV.
 * 
 * Zero TLS overhead - designed for local network use.
 */

#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace RTC {
    struct UdpPusherData;
}

namespace UDPPUSH {

class UdpPusher {
public:
    enum class PushNowResult : uint8_t {
        Queued,
        Sent,
        NotConfigured,
        WorkerStopping,
        WifiDisconnected,
        SendFailed,
    };

    UdpPusher();
    ~UdpPusher();

    /** Initialize runtime state for the RTC-backed UDP configuration. */
    void begin();
    
    /**
     * @brief Update pusher (call from main loop)
     * Handles its own timing based on configured interval.
     */
    void update();
    
    /**
     * @brief Force immediate push of current sensor data
     */
    PushNowResult pushNow();

private:
    bool ensureWorkerStartedLocked();
    bool stopWorkerLocked();
    bool reapStoppedWorkerLocked(TickType_t waitTicks);
    void destroyWorkerResourcesLocked();
    PushNowResult schedulePushLocked(const RTC::UdpPusherData& cfg);
    PushNowResult runPushLocked(const RTC::UdpPusherData& cfg);
    static void taskEntry(void* param);
    void taskLoop();

    SemaphoreHandle_t _stateLock = nullptr;
    uint32_t _startupMs = 0;
    uint32_t _lastPushMs = 0;
    bool _initialized = false;
    char* _buffer = nullptr;
    std::atomic<bool> _pushRequested{false};
    std::atomic<bool> _stopRequested{false};
    TaskHandle_t _taskHandle = nullptr;
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskBuffer = nullptr;
    SemaphoreHandle_t _cleanupSem = nullptr;
    
    /**
     * @brief Send sensor data via UDP
     * @return true if packet sent successfully
     */
    bool doSend(const RTC::UdpPusherData& cfg);
    
    /**
     * @brief Format sensor data as InfluxDB line protocol
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return Number of bytes written
     */
    size_t formatLineProtocol(char* buffer, size_t bufferSize);
    
    /**
     * @brief Format sensor data as JSON
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return Number of bytes written
     */
    size_t formatJson(char* buffer, size_t bufferSize);
    
    /**
     * @brief Format sensor data as CSV
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return Number of bytes written
     */
    size_t formatCsv(char* buffer, size_t bufferSize);
};

}  // namespace UDPPUSH
