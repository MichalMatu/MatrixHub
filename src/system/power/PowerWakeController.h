#pragma once

#include <Arduino.h>
#include <esp_sleep.h>

namespace POWER {

enum class WakeReason : uint8_t {
    Unknown,
    Timer,
    Touch,
    Button,
    Other
};

struct WakeSourceConfig {
    uint32_t wakeIntervalMs = 0;
    uint8_t touchGpio = 0;
    uint32_t touchThreshold = 0;
    bool timerEnabled = false;
    bool buttonEnabled = false;
    bool touchEnabled = false;
};

class PowerWakeController {
public:
    void begin();
    WakeReason getWakeReason();
    esp_sleep_wakeup_cause_t getWakeupCauseRaw();
    uint64_t getGpioWakeupMask();
    uint64_t getExt1WakeupMask();
    bool configureWakeSources(const WakeSourceConfig& config);
    
    // Test hooks
    void setConfigureWakeSourcesCallback(void (*callback)());
    void resetTestHooks();

private:
    WakeReason _wakeReason = WakeReason::Unknown;
    esp_sleep_wakeup_cause_t _wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;
    uint64_t _gpioWakeupMask = 0;
    uint64_t _ext1WakeupMask = 0;
    void (*_configureWakeSourcesCallback)() = nullptr;
};

} // namespace POWER
