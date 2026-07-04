#include "AirMouseConfigAdapter.h"
#include "AirMouseConfig.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/rtc/RtcConfigLoader.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "AirMouseCfg"

namespace AIRMOUSE {

namespace {
bool readAirMouseConfigSnapshot(RTC::AirMouseData& cfg) {
    bool loaded = false;
    RTC::withConfig([&](const RTC::ConfigStore& store) {
        cfg = store.airMouse;
        loaded = true;
    });
    return loaded;
}
} // namespace

AirMouseConfigAdapter::AirMouseConfigAdapter() {
}

AirMouseConfigAdapter::~AirMouseConfigAdapter() {
    if (_configMutex) { vSemaphoreDelete(_configMutex); _configMutex = nullptr; }
}

bool AirMouseConfigAdapter::begin() {
    if (!_configMutex) {
        _configMutex = xSemaphoreCreateMutex();
        if (!_configMutex) {
            LOGE("Failed to create config mutex");
            return false;
        }
    }
    return true;
}

void AirMouseConfigAdapter::applySettings() {
    if (!_configMutex) return;

    RTC::AirMouseData cfg{};
    if (!readAirMouseConfigSnapshot(cfg)) {
        LOGW("RTC AirMouse config unavailable - skipping applySettings");
        return;
    }
    
    SYSTEM::ScopeLock lock(_configMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOCKS::CONFIG_APPLY_TIMEOUT_MS));
    if (!lock.isLocked()) {
        LOGW("Config mutex timeout");
        return;
    }
    
    // Pull from RTC config (single source of truth).
    const bool movementEnabled = cfg.movementEnabled;
    _movementEnabled.store(movementEnabled, std::memory_order_release);
    
    const bool clickEnabled = cfg.clickEnabled;
    const bool prevClickEnabled = _clickEnabled.load(std::memory_order_relaxed);
    if (clickEnabled && !prevClickEnabled) {
        _detectorResetRequested.store(true, std::memory_order_release);
    }
    _clickEnabled.store(clickEnabled, std::memory_order_release);
    
    // Update local filter config (1-Euro filter).
    _sensitivityX = cfg.sensitivityX;
    _sensitivityY = cfg.sensitivityY;
    _deadzone = cfg.deadzone;
    _accelerationEnabled = cfg.accelerationEnabled;
    _accelerationFactor = cfg.accelerationFactor;
    _tapThresholdG = cfg.tapThresholdG;
    _clickDebounceMs = cfg.clickDebounceMs;
    _doubleClickWindowMs = cfg.doubleClickWindowMs;
    _filterMinCutoff = cfg.euroMinCutoff;
    _filterBeta = cfg.euroBeta;
    _filterDCutoff = cfg.euroDCutoff;

    // Signal task loop to re-apply filter parameters.
    _filterParamsChanged.store(true, std::memory_order_release);

    // Click source & actions.
    _clickSource = cfg.clickSource;
    const bool clickSourceSensor = (cfg.clickSource == RTC::ClickSource::SENSOR);
    _singleClickAction = cfg.singleClickAction;
    _doubleClickAction = cfg.doubleClickAction;
    _tripleClickAction = cfg.tripleClickAction;
    
    // Cache script filenames for RUN_SCRIPT action.
    strncpy(_singleClickScript, cfg.singleClickScript, sizeof(_singleClickScript) - 1);
    _singleClickScript[sizeof(_singleClickScript) - 1] = '\0';
    
    strncpy(_doubleClickScript, cfg.doubleClickScript, sizeof(_doubleClickScript) - 1);
    _doubleClickScript[sizeof(_doubleClickScript) - 1] = '\0';
    
    strncpy(_tripleClickScript, cfg.tripleClickScript, sizeof(_tripleClickScript) - 1);
    _tripleClickScript[sizeof(_tripleClickScript) - 1] = '\0';

    _runtimeCfg.jigglerMode = cfg.jiggler.mode;
    _runtimeCfg.jigglerInterval = cfg.jiggler.interval;
    _runtimeCfg.jigglerMoveDistance = cfg.jiggler.moveDistance;
    _runtimeCfg.jigglerRandomInterval = cfg.jiggler.randomInterval;
    _runtimeCfg.clickSource = cfg.clickSource;
    _runtimeCfgDirty.store(true, std::memory_order_release);

    lock.unlock(); // Release early

    // Application owns the physical button instance, so config updates leave
    // this module through a small sink instead of reaching into ButtonHandler.
    if (_physicalButtonDoubleClickWindowSink) {
        _physicalButtonDoubleClickWindowSink(cfg.doubleClickWindowMs);
    }

    // IMU is needed for movement or tap detection (sensor-based clicks).
    const bool needsImu = movementEnabled || (clickEnabled && clickSourceSensor);
    _needsImu.store(needsImu, std::memory_order_release);
    
    // Note: ImuManager setConsumerActive remains in the Task/Controller
    // Note: setEnabled for the whole service remains in the Task/Service
}

bool AirMouseConfigAdapter::getRuntimeConfig(RuntimeCfg& cfg) {
    if (!_configMutex) return false;

    SYSTEM::ScopeLock lock(_configMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOCKS::CONFIG_APPLY_TIMEOUT_MS));
    if (!lock.isLocked()) return false;

    cfg = _runtimeCfg;
    return true;
}

bool AirMouseConfigAdapter::isRuntimeConfigDirty() {
    return _runtimeCfgDirty.load(std::memory_order_acquire);
}

void AirMouseConfigAdapter::clearRuntimeConfigDirty() {
    _runtimeCfgDirty.store(false, std::memory_order_release);
}

bool AirMouseConfigAdapter::hasFilterParamsChanged() {
    return _filterParamsChanged.load(std::memory_order_acquire);
}
void AirMouseConfigAdapter::clearFilterParamsChanged() {
    _filterParamsChanged.store(false, std::memory_order_release);
}

bool AirMouseConfigAdapter::isDetectorResetRequested() {
    return _detectorResetRequested.load(std::memory_order_acquire);
}
void AirMouseConfigAdapter::clearDetectorResetRequested() {
    _detectorResetRequested.store(false, std::memory_order_release);
}

bool AirMouseConfigAdapter::getComponentConfigs(AirMouseConfig& amCfg, FilterConfig& fCfg) {
    if (!_configMutex) return false;

    SYSTEM::ScopeLock lock(_configMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOCKS::CONFIG_UPDATE_TIMEOUT_MS));
    if (!lock.isLocked()) return false;
    
    amCfg.sensitivityX = _sensitivityX;
    amCfg.sensitivityY = _sensitivityY;
    amCfg.deadzone = _deadzone;
    amCfg.accelerationEnabled = _accelerationEnabled;
    amCfg.accelerationFactor = _accelerationFactor;
    amCfg.tapThresholdG = _tapThresholdG;
    amCfg.clickDebounceMs = _clickDebounceMs;
    amCfg.doubleClickWindowMs = _doubleClickWindowMs;
    
    fCfg.minCutoff = _filterMinCutoff;
    fCfg.beta = _filterBeta;
    fCfg.dCutoff = _filterDCutoff;
    return true;
}

bool AirMouseConfigAdapter::getClickSettings(RTC::ClickSource& source,
                                             RTC::ClickAction& singleAction, char* singleScript,
                                             RTC::ClickAction& doubleAction, char* doubleScript,
                                             RTC::ClickAction& tripleAction, char* tripleScript)
{
    if (!_configMutex) return false;

    SYSTEM::ScopeLock lock(_configMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOCKS::BUTTON_CONFIG_TIMEOUT_MS));
    if (!lock.isLocked()) return false;
    
    source = _clickSource;
    singleAction = _singleClickAction;
    doubleAction = _doubleClickAction;
    tripleAction = _tripleClickAction;
    
    if (singleScript) strncpy(singleScript, _singleClickScript, 64);
    if (doubleScript) strncpy(doubleScript, _doubleClickScript, 64);
    if (tripleScript) strncpy(tripleScript, _tripleClickScript, 64);
    return true;
}

void AirMouseConfigAdapter::setPhysicalButtonDoubleClickWindowSink(std::function<void(uint32_t)> sink) {
    // This sink intentionally does not own the target. Application/Phase 7 own
    // the physical button lifetime and only lend us a callback for config
    // propagation.
    _physicalButtonDoubleClickWindowSink = std::move(sink);
}

} // namespace AIRMOUSE
