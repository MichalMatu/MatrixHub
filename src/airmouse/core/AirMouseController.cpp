#include "AirMouseController.h"

#include "../../system/logging/Logging.h"
#include <esp_timer.h>
#include "../../system/utils/ScopeLock.h"
#include "../../config/System.h"
#include "../features/ClickHandler.h"

#undef LOG_TAG
#define LOG_TAG "AirMouseCtrl"

namespace AIRMOUSE {

AirMouseController::AirMouseController(AirMouseConfigAdapter* configAdapter, 
                                       ImuService* imuService, 
                                       IMU::ImuManager* imuManager,
                                       MouseActuator* actuator, 
                                       KeyboardActuator* keyActuator, 
                                       SystemTimeProvider* timeProvider, 
                                       SemaphoreHandle_t hidMutex)
    : _configAdapter(configAdapter), _imuService(imuService), _imuManager(imuManager),
      _actuator(actuator), _keyActuator(keyActuator), _timeProvider(timeProvider), _hidMutex(hidMutex) {
}

AirMouseController::~AirMouseController() {
    if (_jiggler) { delete _jiggler; _jiggler = nullptr; }
    if (_processor) { delete _processor; _processor = nullptr; }
    if (_detector) { delete _detector; _detector = nullptr; }

    if (_callbackMutex) { vSemaphoreDelete(_callbackMutex); _callbackMutex = nullptr; }
}

bool AirMouseController::begin() {
    if (!_configAdapter || !_imuService || !_actuator || !_timeProvider || !_hidMutex) {
        LOGE("Missing dependencies in AirMouseController");
        return false;
    }

    if (!_callbackMutex) {
        _callbackMutex = xSemaphoreCreateMutex();
        if (!_callbackMutex) {
            LOGE("Failed to create callback mutex");
            return false;
        }
    }

    // Allocate and wire internal components.
    if (!_processor) _processor = new (std::nothrow) AirMouseProcessor();
    if (!_detector) _detector = new (std::nothrow) TapDetector();
    if (!_jiggler) _jiggler = new (std::nothrow) JigglerController(*_actuator, *_timeProvider, _keyActuator);
    
    if (!_processor || !_detector || !_jiggler) {
        LOGE("Failed to allocate AirMouse components");
        return false;
    }

    return updateComponentConfigs();
}

void AirMouseController::calibrate() {
    // Async request — actual work runs in the AirMouseTask thread context.
    _calibrationRequested.store(true, std::memory_order_release);
}

void AirMouseController::resetProcessorFilters() {
    if (_processor) _processor->resetFilters();
}

void AirMouseController::resetDetector() {
    if (_detector) _detector->reset();
}

void AirMouseController::notifyUserActivity() {
    if (_jiggler) _jiggler->notifyUserActivity();
}

void AirMouseController::setImuCallback(ImuCallback cb) {
    if (!_callbackMutex) {
        _imuCallback = cb;
        return;
    }
    SYSTEM::ScopeLock lock(_callbackMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOCKS::CONFIG_APPLY_TIMEOUT_MS));
    if (lock.isLocked()) {
        _imuCallback = std::move(cb);
    }
}

void AirMouseController::getDebugSnapshot(AirMouseDebugSnapshot& snapshot) const {
    portENTER_CRITICAL(&_debugSnapshotLock);
    snapshot = _debugSnapshot;
    portEXIT_CRITICAL(&_debugSnapshotLock);
}

void AirMouseController::getGyroOffsets(float& x, float& y, float& z) {
    AirMouseDebugSnapshot snapshot{};
    getDebugSnapshot(snapshot);
    x = snapshot.offsetX;
    y = snapshot.offsetY;
    z = snapshot.offsetZ;
}

float AirMouseController::getLastDeltaG() {
    AirMouseDebugSnapshot snapshot{};
    getDebugSnapshot(snapshot);
    return snapshot.deltaG;
}

void AirMouseController::getLastImuData(float& gx, float& gy, float& gz, float& ax, float& ay, float& az) {
    AirMouseDebugSnapshot snapshot{};
    getDebugSnapshot(snapshot);
    gx = snapshot.gx;
    gy = snapshot.gy;
    gz = snapshot.gz;
    ax = snapshot.ax;
    ay = snapshot.ay;
    az = snapshot.az;
}

void AirMouseController::updateDebugSnapshot(const AirMouseDebugSnapshot& snapshot) {
    portENTER_CRITICAL(&_debugSnapshotLock);
    _debugSnapshot = snapshot;
    portEXIT_CRITICAL(&_debugSnapshotLock);
}

void AirMouseController::updateDebugOffsets(float x, float y, float z) {
    portENTER_CRITICAL(&_debugSnapshotLock);
    _debugSnapshot.offsetX = x;
    _debugSnapshot.offsetY = y;
    _debugSnapshot.offsetZ = z;
    portEXIT_CRITICAL(&_debugSnapshotLock);
}

void AirMouseController::doCalibration() {
    LOGI("Calibrating...");
    _calibrationInProgress = true;
    
    // Average multiple gyro samples from the cached IMU data.
    float sumX = 0, sumY = 0, sumZ = 0;
    int validSamples = 0;

    for (uint32_t i = 0; i < CONFIG::TASKS::AIR_MOUSE_CALIB_SAMPLES; i++) {
        float ax, ay, az, gx, gy, gz;
        if (_imuService && _imuService->getCachedSample(ax, ay, az, gx, gy, gz)) {
            sumX += gx; sumY += gy; sumZ += gz;
            validSamples++;
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG::TASKS::AIR_MOUSE_CALIB_SAMPLE_DELAY_MS));
    }

    if (validSamples > 0 && _processor) {
        float offX = sumX / validSamples;
        float offY = sumY / validSamples;
        float offZ = sumZ / validSamples;
        _processor->calibrate(offX, offY, offZ);
        updateDebugOffsets(offX, offY, offZ);
        LOGI("Offsets: %.2f %.2f %.2f", offX, offY, offZ);
    } else if (validSamples == 0) {
        LOGW("Calibration: no cached IMU samples available — skipped");
    }

    _calibrationInProgress = false;
}

bool AirMouseController::updateComponentConfigs() {
    if (!_processor || !_detector || !_configAdapter) return false;
    
    // Pull latest RTC config into local copies.
    AirMouseConfig amCfg{};
    FilterConfig fCfg{};
    if (!_configAdapter->getComponentConfigs(amCfg, fCfg)) {
        LOGW("Component config snapshot timed out");
        return false;
    }

    _processor->updateConfig(amCfg, fCfg);
    _detector->updateConfig(amCfg);
    return true;
}

void AirMouseController::updateJiggler(const AirMouseConfigAdapter::RuntimeCfg& cfgState) {
    if (!_jiggler) return;

    // Convert cached runtime config to JigglerData payload.
    RTC::JigglerData cfg;
    cfg.mode = cfgState.jigglerMode;
    cfg.interval = cfgState.jigglerInterval;
    cfg.moveDistance = cfgState.jigglerMoveDistance;
    cfg.randomInterval = cfgState.jigglerRandomInterval;
    
    _jiggler->update(cfg);
}

void AirMouseController::processLoopStep(uint32_t now) {
    // Handle calibration request in task thread context (avoids data race).
    if (_calibrationRequested.exchange(false, std::memory_order_acq_rel)) {
        doCalibration();
        if (_processor) _processor->resetFilters();
    }

    // Apply component config updates only when a dirty flag is set.
    if (_configAdapter->hasFilterParamsChanged()) {
        if (updateComponentConfigs()) {
            _configAdapter->clearFilterParamsChanged();
        }
    }

    if (_configAdapter->isDetectorResetRequested()) {
        resetDetector();
        _configAdapter->clearDetectorResetRequested();
    }

    // Only lock config mutex when config actually changed.
    if (_configAdapter->isRuntimeConfigDirty()) {
        AirMouseConfigAdapter::RuntimeCfg runtimeCfg{};
        if (_configAdapter->getRuntimeConfig(runtimeCfg)) {
            _cachedRuntimeCfg = runtimeCfg;
            _configAdapter->clearRuntimeConfigDirty();
        } else {
            LOGW("Runtime config snapshot timed out");
        }
    }

    const bool movementEnabled = _configAdapter->isMovementEnabled();
    if (!movementEnabled && _lastMovementEnabled && _actuator && _hidMutex) {
        SYSTEM::ScopeLock lock(_hidMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::HID::LOCK_TIMEOUT_MS));
        if (lock.isLocked()) {
            _actuator->resetAccum();
        }
    }
    _lastMovementEnabled = movementEnabled;

    // Update Jiggler logic safely under HID mutex (shares USB HID with mouse).
    if (_hidMutex) {
        SYSTEM::ScopeLock lock(_hidMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::HID::LOCK_TIMEOUT_MS));
        if (lock.isLocked()) {
            updateJiggler(_cachedRuntimeCfg);
        }
    }

    const bool needsImu = _configAdapter->needsImu();
    if (!needsImu) {
        // Nothing to process IMU-wise, let the task sleep
        return;
    }

    float ax, ay, az, gx, gy, gz;
    LOG_PROFILE_START(mouseLoopUs);

    if (_imuService && _imuService->getCachedSample(ax, ay, az, gx, gy, gz)) {
        MouseMovement mv{0.0f, 0.0f};
        if (_processor) {
            mv = _processor->process(gx, gy, gz, ax, ay, az, now);
        }

        if (movementEnabled && (mv.x != 0.0f || mv.y != 0.0f)) {
            if (_jiggler) _jiggler->notifyUserActivity();

            if (_hidMutex) {
                SYSTEM::ScopeLock lock(_hidMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::HID::LOCK_TIMEOUT_MS));
                if (lock.isLocked()) {
                    // Send HID move with subpixel accumulation & clamp scaling.
                    _actuator->movePrecise(mv.x, mv.y);
                }
            }
        }

        const bool clickEnabled = _configAdapter->isClickEnabled();
        if (clickEnabled && _detector && _cachedRuntimeCfg.clickSource == RTC::ClickSource::SENSOR) {
            // Tap detector uses accelerometer magnitude to detect taps.
            TapGesture gesture = _detector->process(ax, ay, az, now);
            if (gesture != TapGesture::NONE) {
                if (_jiggler) _jiggler->notifyUserActivity();
                if (_clickHandler) {
                    _clickHandler->handleClick(RTC::ClickSource::SENSOR, static_cast<uint8_t>(gesture));
                }
            }
        }

        AirMouseDebugSnapshot snapshot{};
        if (_processor) {
            _processor->getOffsets(snapshot.offsetX, snapshot.offsetY, snapshot.offsetZ);
            _processor->getStats(snapshot.gx, snapshot.gy, snapshot.gz, snapshot.ax, snapshot.ay, snapshot.az);
        }
        if (_detector) {
            snapshot.deltaG = _detector->getLastDeltaG();
        }
        snapshot.timestampMs = now;
        updateDebugSnapshot(snapshot);

        // Optional debug callback (thread-safe access).
        ImuCallback cb;
        if (_callbackMutex) {
            SYSTEM::ScopeLock lock(_callbackMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::LOCKS::CONFIG_APPLY_TIMEOUT_MS));
            if (lock.isLocked()) {
                cb = _imuCallback;
            }
        } else {
            cb = _imuCallback;
        }

        if (cb) {
            // Caller receives filtered gyro/acc + last deltaG (tap signal).
            cb(
                snapshot.gx,
                snapshot.gy,
                snapshot.gz,
                snapshot.ax,
                snapshot.ay,
                snapshot.az,
                snapshot.deltaG);
        }
    } else {
         static uint32_t lastErr = 0;
         if (now - lastErr > CONFIG::TASKS::AIR_MOUSE_IMU_ERROR_LOG_THROTTLE_MS) {
            bool init = _imuService ? _imuService->isInitialized() : false;
            LOGD("IMU cached sample unavailable! Init=%d", init);
            lastErr = now;
         }
    }

    LOG_PROFILE_END_PERIODIC(
        mouseLoopUs,
        "AirMouse process",
        TASK_MONITOR::INTERVAL_AIRMOUSE_MS);
}

} // namespace AIRMOUSE
