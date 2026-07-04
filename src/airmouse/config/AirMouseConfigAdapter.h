#pragma once

#include <Arduino.h>
#include <atomic>
#include <cstring>
#include <functional>
#include <utility>
#include "../../system/rtc/RtcConfig.h" // For JigglerMode, ClickSource, ClickAction

namespace AIRMOUSE {

// Forward declarations
struct AirMouseConfig;
struct FilterConfig;

/**
 * @brief Thread-safe adapter between RTC config and runtime components.
 *
 * - applySettings() copies RTC config into cached fields and flips dirty flags.
 * - getRuntimeConfig()/getComponentConfigs() read cached data under a mutex.
 * - Read methods return false on mutex timeout so callers can keep the last good snapshot.
 * - Atomic flags allow the task loop to check "dirty" state without locking.
 */
class AirMouseConfigAdapter {
public:
    struct RuntimeCfg {
        // Minimal set used inside the task loop (no heavy copies).
        RTC::MouseJigglerMode jigglerMode = RTC::MouseJigglerMode::JIGGLER_OFF;
        uint32_t jigglerInterval = 0;
        uint16_t jigglerMoveDistance = 0;
        bool jigglerRandomInterval = false;
        RTC::ClickSource clickSource = RTC::ClickSource::SENSOR;
    };

    AirMouseConfigAdapter();
    ~AirMouseConfigAdapter();

    bool begin();

    // Trigger an update from RTC/System config.
    void applySettings();

    // Thread-safe access for the main loop.
    bool getRuntimeConfig(RuntimeCfg& cfg);
    bool isRuntimeConfigDirty();
    void clearRuntimeConfigDirty();

    bool hasFilterParamsChanged();
    void clearFilterParamsChanged();

    bool isDetectorResetRequested();
    void clearDetectorResetRequested();

    // Retrieve specific configurations for components.
    bool getComponentConfigs(AirMouseConfig& amCfg, FilterConfig& fCfg);

    // Get current click settings (thread-safe against _configMutex).
    bool getClickSettings(RTC::ClickSource& source,
                          RTC::ClickAction& singleAction, char* singleScript,
                          RTC::ClickAction& doubleAction, char* doubleScript,
                          RTC::ClickAction& tripleAction, char* tripleScript);

    // Physical button timing is owned by Phase 7 / Application, but AirMouse
    // still owns the user-facing config that defines the multi-click window.
    void setPhysicalButtonDoubleClickWindowSink(std::function<void(uint32_t)> sink);

    // Status flags exposed without locking for fast checks.
    bool isMovementEnabled() const { return _movementEnabled.load(std::memory_order_acquire); }
    bool isClickEnabled() const { return _clickEnabled.load(std::memory_order_acquire); }
    bool needsImu() const { return _needsImu.load(std::memory_order_acquire); }

private:
    float _sensitivityX = CONFIG::AIR_MOUSE::DEFAULT_SENSITIVITY_X;
    float _sensitivityY = CONFIG::AIR_MOUSE::DEFAULT_SENSITIVITY_Y;
    float _deadzone = CONFIG::AIR_MOUSE::DEFAULT_DEADZONE;
    bool _accelerationEnabled = CONFIG::AIR_MOUSE::DEFAULT_ACCELERATION_ENABLED;
    float _accelerationFactor = CONFIG::AIR_MOUSE::DEFAULT_ACCELERATION_FACTOR;
    float _tapThresholdG = CONFIG::AIR_MOUSE::DEFAULT_TAP_THRESHOLD_G;
    uint16_t _clickDebounceMs = CONFIG::AIR_MOUSE::DEFAULT_CLICK_DEBOUNCE_MS;
    uint16_t _doubleClickWindowMs = CONFIG::AIR_MOUSE::DEFAULT_DOUBLE_CLICK_WINDOW_MS;

    std::atomic<bool> _movementEnabled{CONFIG::AIR_MOUSE::DEFAULT_MOVEMENT_ENABLED};
    std::atomic<bool> _clickEnabled{CONFIG::AIR_MOUSE::DEFAULT_CLICK_ENABLED};
    std::atomic<bool> _needsImu{false};

    RuntimeCfg _runtimeCfg;
    std::atomic<bool> _runtimeCfgDirty{false};
    std::atomic<bool> _detectorResetRequested{false};
    std::atomic<bool> _filterParamsChanged{false};

    // Filter Config cache
    float _filterMinCutoff = 0.0f;
    float _filterBeta = 0.0f;
    float _filterDCutoff = 0.0f;

    // Click source & action cache (from RTC config).
    RTC::ClickSource _clickSource = RTC::ClickSource::SENSOR;
    RTC::ClickAction _singleClickAction = RTC::ClickAction::LEFT_CLICK;
    RTC::ClickAction _doubleClickAction = RTC::ClickAction::RIGHT_CLICK;
    RTC::ClickAction _tripleClickAction = RTC::ClickAction::NONE;

    char _singleClickScript[64] = {0};
    char _doubleClickScript[64] = {0};
    char _tripleClickScript[64] = {0};

    // Thread safety for config access.
    SemaphoreHandle_t _configMutex = nullptr;
    std::function<void(uint32_t)> _physicalButtonDoubleClickWindowSink;
};

} // namespace AIRMOUSE
