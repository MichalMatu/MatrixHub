/**
 * @file WifiSensingSettings.cpp
 * @brief WiFi Sensing settings service - backed by RTC memory
 * 
 * Refactored 3 Jan 2026: Uses centralized ConfigManager for persistence.
 */

#include "WifiSensingSettings.h"
#include "WifiSensingService.h"
#include "csi/core/CsiService.h"
#include "../config/json/WifiSensingConfigJson.h"
#include "../system/logging/Logging.h"

#include <cstring>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "WifiSenseSettings"

namespace WIFISENSING {
namespace {

constexpr uint32_t kRuntimeReconcileBaseMs = 5000;
constexpr uint32_t kRuntimeReconcileMaxMs = 60000;
constexpr TickType_t kRuntimeApplyLockTimeout = pdMS_TO_TICKS(5000);
constexpr TickType_t kRuntimeReconcileApplyLockTimeout = pdMS_TO_TICKS(50);

CSI::CsiMotionConfig toCsiMotionConfig(const RTC::WifiSensingData& state) {
    CSI::CsiMotionConfig config;
    config.enabled = state.csiAlarmEnabled;
    config.bandCount = state.csiAlarmBandCount > CSI::MAX_CSI_ALARM_BANDS
                           ? CSI::MAX_CSI_ALARM_BANDS
                           : state.csiAlarmBandCount;
    for (uint8_t i = 0; i < config.bandCount; ++i) {
        config.bands[i].start = state.csiAlarmBandStart[i];
        config.bands[i].end = state.csiAlarmBandEnd[i];
    }
    config.baselineFrames = state.csiBaselineFrames;
    config.topK = state.csiTopK;
    config.enterThreshold = state.csiEnterThreshold;
    config.clearThreshold = state.csiClearThreshold;
    config.holdMs = state.csiHoldMs;
    config.clearHoldMs = state.csiClearHoldMs;
    config.minNoise = state.csiMinNoise;
    config.minEnergy = state.csiMinEnergy;
    config.noisyScoreThreshold = state.csiNoisyThreshold;
    config.autoRecalibration = state.csiAutoRecalibration;
    config.sensitivity = state.csiSensitivity;
    return config;
}

bool csiAlarmConfigChanged(const RTC::WifiSensingData& a, const RTC::WifiSensingData& b) {
    if (a.csiAlarmEnabled != b.csiAlarmEnabled ||
        a.csiAlarmBandCount != b.csiAlarmBandCount ||
        a.csiBaselineFrames != b.csiBaselineFrames ||
        a.csiTopK != b.csiTopK ||
        a.csiEnterThreshold != b.csiEnterThreshold ||
        a.csiClearThreshold != b.csiClearThreshold ||
        a.csiHoldMs != b.csiHoldMs ||
        a.csiClearHoldMs != b.csiClearHoldMs ||
        a.csiMinNoise != b.csiMinNoise ||
        a.csiMinEnergy != b.csiMinEnergy ||
        a.csiNoisyThreshold != b.csiNoisyThreshold ||
        a.csiAutoRecalibration != b.csiAutoRecalibration ||
        a.csiSensitivity != b.csiSensitivity) {
        return true;
    }

    for (uint8_t i = 0; i < 4; ++i) {
        if (a.csiAlarmBandStart[i] != b.csiAlarmBandStart[i] ||
            a.csiAlarmBandEnd[i] != b.csiAlarmBandEnd[i]) {
            return true;
        }
    }

    return false;
}

bool wifiRuntimeConfigChanged(const RTC::WifiSensingData& a,
                              const RTC::WifiSensingData& b) {
    return a.enabled != b.enabled ||
           a.sampleIntervalMs != b.sampleIntervalMs ||
           a.varianceThreshold != b.varianceThreshold;
}

} // namespace

WifiSensingSettings::WifiSensingSettings(FS* fs,
                                         WIFISENSING::WifiSensingService* service,
                                         WIFISENSING::CSI::CsiService* csiService)
    : RtcStatefulService(&RTC::ConfigStore::wifiSensing),
      _fs(fs),
      _service(service),
      _csiService(csiService),
      _runtimeReconcileSchedule(kRuntimeReconcileBaseMs,
                                kRuntimeReconcileMaxMs) {
    _runtimeApplyMutex = xSemaphoreCreateMutex();
    if (!_runtimeApplyMutex) {
        LOGE("Failed to create WiFi sensing runtime apply mutex");
    }
    _runtimeReconcileLifecycleMutex = xSemaphoreCreateMutex();
    _runtimeReconcileStopped = xSemaphoreCreateBinary();
    if (!_runtimeReconcileLifecycleMutex || !_runtimeReconcileStopped) {
        LOGE("Failed to create WiFi sensing reconciler lifecycle resources");
    }
    
    addUpdateHandler([this](std::string_view originId) { 
        (void)originId;
        return onConfigUpdated();
    }, false);
}

WifiSensingSettings::~WifiSensingSettings() {
    bool retryLogged = false;
    while (!shutdownRuntimeReconciler(portMAX_DELAY)) {
        if (!retryLogged) {
            LOGW("Waiting for WiFi sensing reconciler before destruction");
            retryLogged = true;
        }
        vTaskDelay(TIMEOUT::TASK_SHUTDOWN_POLL_TICKS);
    }
    if (_runtimeReconcileStopped) {
        vSemaphoreDelete(_runtimeReconcileStopped);
        _runtimeReconcileStopped = nullptr;
    }
    if (_runtimeReconcileLifecycleMutex) {
        vSemaphoreDelete(_runtimeReconcileLifecycleMutex);
        _runtimeReconcileLifecycleMutex = nullptr;
    }
    if (_runtimeApplyMutex) {
        vSemaphoreDelete(_runtimeApplyMutex);
        _runtimeApplyMutex = nullptr;
    }
}

void WifiSensingSettings::begin() {
    _lastPersistedState = _state;
    
    // Data is already in RTC
    LOGI("Settings (RTC): enabled=%d, interval=%ums, threshold=%.2f, csi_alarm=%d",
         _state.enabled ? 1 : 0,
         _state.sampleIntervalMs,
         _state.varianceThreshold,
         _state.csiAlarmEnabled ? 1 : 0);
    
    // Start service if enabled
    if (_state.enabled && _service) {
        if (!_service->begin(_state.sampleIntervalMs, _state.varianceThreshold)) {
            LOGE("Failed to apply persisted WiFi sensing state during boot");
        }
    }

    if (_csiService) {
        if (!_csiService->setMotionConfig(toCsiMotionConfig(_state))) {
            LOGE("Failed to apply persisted CSI alarm state during boot");
        }
    }

    markRuntimeReconcileHealthy(millis());
    SYSTEM::ScopeLock lifecycleLock(
        _runtimeReconcileLifecycleMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
    if (!lifecycleLock.isLocked() ||
        !ensureRuntimeReconcileWorkerLocked()) {
        markRuntimeReconcileFailure(millis());
        LOGW("WiFi sensing runtime reconciler will retry task creation");
    }
}

void WifiSensingSettings::reconcileRuntimeIfDue(uint32_t nowMs) {
    // This method runs in loopTask and must stay bounded. All RTC reads, service
    // locks and worker start/stop waits execute in the persistent task below.
    if (!_runtimeReconcileWorkerGate.shouldRun() ||
        !isRuntimeReconcileDue(nowMs)) {
        return;
    }

    // A zero-wait lifecycle lock keeps this tick deterministic if shutdown or a
    // one-time task creation is already in progress on the other core.
    SYSTEM::ScopeLock lifecycleLock(_runtimeReconcileLifecycleMutex, 0);
    if (!lifecycleLock.isLocked() ||
        !_runtimeReconcileWorkerGate.shouldRun()) {
        return;
    }
    if (!ensureRuntimeReconcileWorkerLocked()) {
        markRuntimeReconcileFailure(millis());
        LOGW("Failed to start WiFi sensing runtime reconciler; retry deferred");
        return;
    }

    if (_runtimeReconcileTaskHandle) {
        xTaskNotifyGive(_runtimeReconcileTaskHandle);
    }
}

bool WifiSensingSettings::shutdownRuntimeReconciler(TickType_t waitTicks) {
    _runtimeReconcileWorkerGate.requestShutdown();

    if (!_runtimeReconcileWorkerGate.isWorkerRunning()) {
        return true;
    }

    SYSTEM::ScopeLock lifecycleLock(
        _runtimeReconcileLifecycleMutex, waitTicks);
    if (!lifecycleLock.isLocked()) {
        return false;
    }

    if (_runtimeReconcileTaskHandle) {
        xTaskNotifyGive(_runtimeReconcileTaskHandle);
    }
    return reapRuntimeReconcileWorkerLocked(waitTicks);
}

bool WifiSensingSettings::isRuntimeReconcileDue(uint32_t nowMs) {
    bool due = false;
    portENTER_CRITICAL(&_runtimeReconcileScheduleMux);
    due = _runtimeReconcileSchedule.isDue(nowMs);
    portEXIT_CRITICAL(&_runtimeReconcileScheduleMux);
    return due;
}

void WifiSensingSettings::markRuntimeReconcileHealthy(uint32_t completedAtMs) {
    portENTER_CRITICAL(&_runtimeReconcileScheduleMux);
    _runtimeReconcileSchedule.markHealthy(completedAtMs);
    portEXIT_CRITICAL(&_runtimeReconcileScheduleMux);
}

void WifiSensingSettings::markRuntimeReconcileFailure(uint32_t completedAtMs) {
    portENTER_CRITICAL(&_runtimeReconcileScheduleMux);
    _runtimeReconcileSchedule.markFailure(completedAtMs);
    portEXIT_CRITICAL(&_runtimeReconcileScheduleMux);
}

bool WifiSensingSettings::ensureRuntimeReconcileWorkerLocked() {
    if (_runtimeReconcileTaskHandle) {
        return true;
    }
    if (!_runtimeReconcileStopped ||
        !_runtimeReconcileWorkerGate.tryClaim()) {
        return false;
    }

    (void)xSemaphoreTake(_runtimeReconcileStopped, 0);
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        runtimeReconcileTask,
        "wifi_reconcile",
        CONFIG::TASKS::STACK_WIFI_SENSING_RECONCILE,
        this,
        CONFIG::TASKS::PRIO_WIFI_SENSING_RECONCILE,
        &taskHandle,
        CONFIG::TASKS::CORE_WIFI_SENSING_RECONCILE);
    if (created != pdPASS || !taskHandle) {
        _runtimeReconcileWorkerGate.complete();
        return false;
    }

    _runtimeReconcileTaskHandle = taskHandle;
    return true;
}

bool WifiSensingSettings::reapRuntimeReconcileWorkerLocked(
    TickType_t waitTicks) {
    if (!_runtimeReconcileTaskHandle) {
        _runtimeReconcileWorkerGate.complete();
        return true;
    }
    if (xTaskGetCurrentTaskHandle() == _runtimeReconcileTaskHandle) {
        return false;
    }

    if (eTaskGetState(_runtimeReconcileTaskHandle) != eSuspended) {
        if (!_runtimeReconcileStopped ||
            (xSemaphoreTake(_runtimeReconcileStopped, waitTicks) != pdTRUE &&
             eTaskGetState(_runtimeReconcileTaskHandle) != eSuspended)) {
            return false;
        }

        const TickType_t pollTicks = pdMS_TO_TICKS(5) > 0
                                         ? pdMS_TO_TICKS(5)
                                         : 1;
        const TickType_t settleTicks = pdMS_TO_TICKS(50);
        TickType_t waitedTicks = 0;
        while (eTaskGetState(_runtimeReconcileTaskHandle) != eSuspended &&
               waitedTicks < settleTicks) {
            vTaskDelay(pollTicks);
            waitedTicks += pollTicks;
        }
    }

    if (eTaskGetState(_runtimeReconcileTaskHandle) != eSuspended) {
        return false;
    }

    vTaskDelete(_runtimeReconcileTaskHandle);
    _runtimeReconcileTaskHandle = nullptr;
    _runtimeReconcileWorkerGate.complete();
    return true;
}

void WifiSensingSettings::runtimeReconcileTask(void* context) {
    auto* self = static_cast<WifiSensingSettings*>(context);
    if (!self) {
        vTaskDelete(nullptr);
        return;
    }

    while (self->_runtimeReconcileWorkerGate.shouldRun()) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!self->_runtimeReconcileWorkerGate.shouldRun()) {
            break;
        }
        if (self->isRuntimeReconcileDue(millis())) {
            self->runRuntimeReconcileAttempt();
        }
    }

    if (self->_runtimeReconcileStopped) {
        xSemaphoreGive(self->_runtimeReconcileStopped);
    }
    vTaskSuspend(nullptr);
}

void WifiSensingSettings::runRuntimeReconcileAttempt() {
    // Snapshot desired RTC state under the inherited service lock, then release
    // it before lifecycle operations that can wait for FreeRTOS workers.
    const uint32_t configGeneration = _runtimeConfigFence.snapshot();
    RTC::WifiSensingData desiredState{};
    const StateHandlerResult readResult = read(
        [&desiredState](RTC::WifiSensingData& state) { desiredState = state; });
    if (!readResult.ok) {
        markRuntimeReconcileFailure(millis());
        return;
    }

    // Never call read() while holding this mutex: HTTP updates hold the
    // inherited RTC service mutex while entering onConfigUpdated(), so reversing
    // that order would deadlock. The generation check below rejects a snapshot
    // if an update entered between the read and this serialized apply section.
    SYSTEM::ScopeLock runtimeLock(
        _runtimeApplyMutex, kRuntimeReconcileApplyLockTimeout);
    if (!runtimeLock.isLocked()) {
        markRuntimeReconcileFailure(millis());
        return;
    }
    if (!_runtimeConfigFence.isCurrent(configGeneration)) {
        markRuntimeReconcileHealthy(millis());
        return;
    }
    if (!_runtimeReconcileWorkerGate.shouldRun()) {
        return;
    }

    bool healthy = true;
    bool attemptedRepair = false;

    if (_service && _service->needsRuntimeReconcile(
                        desiredState.enabled,
                        desiredState.sampleIntervalMs,
                        desiredState.varianceThreshold)) {
        attemptedRepair = true;
        const bool wifiHealthy = _service->reconcileRuntime(
            desiredState.enabled,
            desiredState.sampleIntervalMs,
            desiredState.varianceThreshold);
        healthy = wifiHealthy && healthy;
    }

    if (_runtimeReconcileWorkerGate.shouldRun() &&
        _csiService && _csiService->needsRuntimeReconcile()) {
        attemptedRepair = true;
        const bool csiHealthy = _csiService->reconcileRuntime();
        healthy = csiHealthy && healthy;
    }

    if (healthy) {
        if (attemptedRepair) {
            LOGI("WiFi sensing runtime reconciled with persisted state");
        }
        markRuntimeReconcileHealthy(millis());
        return;
    }

    LOGW("WiFi sensing runtime reconciliation deferred");
    markRuntimeReconcileFailure(millis());
}

void WifiSensingSettings::readState(RTC::WifiSensingData& settings, JsonObject& root) {
    CONFIG::JSON::serializeWifiSensing(root, settings);
}

StateUpdateResult WifiSensingSettings::updateState(
    JsonObject& jsonObject,
    RTC::WifiSensingData& settings,
    std::string_view originId) {
    (void)originId;
    RTC::WifiSensingData nextState = settings;
    CONFIG::JSON::deserializeWifiSensing(jsonObject, nextState);

    if (memcmp(&settings, &nextState, sizeof(RTC::WifiSensingData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult WifiSensingSettings::onConfigUpdated() {
    // Publish the generation before waiting for runtime serialization. A
    // reconciler that captured the old RTC state will then either defer, or
    // finish first and be followed by this newer authoritative transaction.
    (void)_runtimeConfigFence.markConfigChanged();
    SYSTEM::ScopeLock runtimeLock(_runtimeApplyMutex, kRuntimeApplyLockTimeout);
    if (!runtimeLock.isLocked()) {
        LOGE("Failed to serialize WiFi sensing runtime update");
        return StateHandlerResult::failure("config/apply_busy");
    }

    const RTC::WifiSensingData previousState = _lastPersistedState;
    const RTC::WifiSensingData nextState = _state;

    const auto applyWifiRuntime = [this](const RTC::WifiSensingData& fromState,
                                        const RTC::WifiSensingData& toState) -> bool {
        if (_service) {
            if (fromState.enabled != toState.enabled) {
                if (toState.enabled) {
                    LOGI("Enabling WiFi Sensing...");
                    if (!_service->begin(toState.sampleIntervalMs, toState.varianceThreshold)) {
                        return false;
                    }
                } else {
                    LOGI("Disabling WiFi Sensing...");
                    if (!_service->stop()) {
                        return false;
                    }
                }
            } else if (toState.enabled &&
                       (wifiRuntimeConfigChanged(fromState, toState) ||
                        !_service->isRunning())) {
                // Reconfigure the live worker in place. If runtime drift already left
                // the service stopped, start it fresh instead of reporting a false
                // "saved" success while sensing stays dead until reboot.
                LOGI("Updating WiFi Sensing settings...");
                if (!_service->isRunning()) {
                    if (!_service->begin(toState.sampleIntervalMs, toState.varianceThreshold)) {
                        return false;
                    }
                } else if (!_service->stop() ||
                           !_service->begin(toState.sampleIntervalMs, toState.varianceThreshold)) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto applyCsiRuntime = [this](const RTC::WifiSensingData& fromState,
                                       const RTC::WifiSensingData& toState) -> bool {
        if (_csiService && csiAlarmConfigChanged(fromState, toState)) {
            if (!_csiService->setMotionConfig(toCsiMotionConfig(toState))) {
                return false;
            }
        }

        return true;
    };

    const auto applyRuntimeState = [&](const RTC::WifiSensingData& fromState,
                                       const RTC::WifiSensingData& toState) -> bool {
        if (!applyWifiRuntime(fromState, toState)) {
            if (!applyWifiRuntime(toState, fromState)) {
                LOGE("Failed to roll back partial WiFi sensing runtime update");
            }
            return false;
        }

        if (!applyCsiRuntime(fromState, toState)) {
            const bool csiRolledBack = applyCsiRuntime(toState, fromState);
            const bool wifiRolledBack = applyWifiRuntime(toState, fromState);
            if (!csiRolledBack || !wifiRolledBack) {
                LOGE("Failed to roll back partial WiFi/CSI runtime update");
            }
            return false;
        }

        return true;
    };

    LOGI("Settings updated: enabled=%d, interval=%ums, threshold=%.2f, csi_alarm=%d",
         nextState.enabled ? 1 : 0,
         nextState.sampleIntervalMs,
         nextState.varianceThreshold,
         nextState.csiAlarmEnabled ? 1 : 0);

    // Apply runtime first. If that fails, the outer transactional RTC service
    // can still roll the in-memory/RTC state back without leaving the live
    // worker silently diverged from the requested config.
    if (!applyRuntimeState(previousState, nextState)) {
        LOGE("Failed to apply WiFi sensing runtime state");
        return StateHandlerResult::failure("config/apply_failed");
    }

    // Persist only after runtime accepted the transition. If the filesystem
    // save fails, revert the live runtime back to the last known persisted
    // state so RTC rollback and runtime behavior stay aligned.
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist WiFi sensing settings");
        if (!applyRuntimeState(nextState, previousState)) {
            LOGE("Failed to roll back WiFi sensing runtime after save failure");
        }
        return StateHandlerResult::failure("config/save_failed");
    }

    _lastPersistedState = nextState;
    return StateHandlerResult::success();
}

bool WifiSensingSettings::isEnabled() const {
    return _state.enabled;
}

uint16_t WifiSensingSettings::getSampleIntervalMs() const {
    return _state.sampleIntervalMs;
}

float WifiSensingSettings::getVarianceThreshold() const {
    return _state.varianceThreshold;
}

}  // namespace WIFISENSING
