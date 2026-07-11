#pragma once

#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <functional>
#include <utility>

#include "../system/utils/ScopeLock.h"
#include "GpioTypes.h"

namespace GPIO {

class GpioService {
public:
    using InputChangeCallback = std::function<void(const char*, bool)>;

    explicit GpioService(FS* fs = nullptr);
    ~GpioService();

    bool begin();
    void stop();
    bool isRunning() const { return _running; }

    bool getConfig(GpioData& out) const;
    bool getStatus(GpioChannelStatus* out, uint8_t maxItems, uint8_t& outCount) const;
    bool getChannelStatus(const char* id, GpioChannelStatus& out) const;
    bool getLogicalValue(const char* id, bool& outValue) const;
    bool setInputChangeCallback(InputChangeCallback callback);

    bool updateConfig(const GpioData& nextConfig, bool persist = true);
    bool setOutput(const char* id, bool logicalValue, bool persist = false);

#ifdef UNIT_TEST
    void sampleInputsOnceForTest(uint32_t nowMs) {
        sampleAndPublishInputs(nowMs);
    }
    bool applyRuntimeConfigForTest(const GpioData& config, uint32_t nowMs);
#endif

private:
    struct LogicalChange {
        char id[kMaxIdLen]{};
        bool logicalValue = false;
    };

    struct RuntimeChannel {
        GpioChannelConfig config;
        bool configured = false;
        bool rawLevel = false;
        bool lastRawLevel = false;
        bool stableRawLevel = false;
        bool logicalLevel = false;
        bool pendingRawLevel = false;
        bool stable = false;
        uint32_t rawChangedAtMs = 0;
        uint32_t changedAtMs = 0;
        uint32_t sampledAtMs = 0;
    };

    static void taskEntry(void* arg);

    bool ensureLock(const SYSTEM::ScopeLock& lock, const char* operation) const;
    bool persistConfig() const;
    void applyConfigLocked(const GpioData& config, uint32_t nowMs);
    void applyConfigAndPublishLocked(const GpioData& config, uint32_t nowMs);
    void configureChannelLocked(RuntimeChannel& channel, uint32_t nowMs);
    void sampleAndPublishInputs(uint32_t nowMs);
    void sampleInputsLocked(uint32_t nowMs,
                            LogicalChange* changes,
                            uint8_t& changeCount);
    bool sampleInputLocked(RuntimeChannel& channel, uint32_t nowMs);
    uint8_t snapshotLogicalLevelsLocked(LogicalChange* changes,
                                        uint8_t maxChanges) const;
    void dispatchChangesLocked(const LogicalChange* changes,
                               uint8_t changeCount);
    int findChannelIndexLocked(const char* id) const;
    static bool logicalFromRaw(bool raw, bool inverted);
    static int arduinoPinModeFor(const GpioChannelConfig& config);

    FS* _fs;
    mutable SemaphoreHandle_t _mutex = nullptr;
    RuntimeChannel _channels[kMaxChannels];
    uint8_t _channelCount = 0;
    TaskHandle_t _taskHandle = nullptr;
    bool _running = false;
    InputChangeCallback _inputChangeCallback = nullptr;
};

}  // namespace GPIO
