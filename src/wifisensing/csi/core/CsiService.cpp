#include <Arduino.h>
#include "CsiService.h"
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <new>
#include "../../../system/logging/Logging.h" 
#include "../../../system/utils/ScopeLock.h"
#include "CsiMotionCalibrationGate.h"
#include "CsiRuntimeCleanupGate.h"
#include "CsiRuntimeReadiness.h"
#include "config/System.h"

#undef LOG_TAG
#define LOG_TAG "CsiService"

namespace WIFISENSING {
namespace CSI {
namespace {

uint8_t countActiveConsumers(uint32_t mask) {
    uint8_t count = 0;
    while (mask != 0) {
        count += static_cast<uint8_t>(mask & 1u);
        mask >>= 1;
    }
    return count;
}

} // namespace

CsiService::CsiService() {
    _stateMutex = xSemaphoreCreateMutex();
    _callbackMutex = xSemaphoreCreateMutex();
    _motionCallbackMutex = xSemaphoreCreateMutex();
    _motionPublishMutex = xSemaphoreCreateMutex();
    _motionConfigMutex = xSemaphoreCreateMutex();
    _motionControlMutex = xSemaphoreCreateMutex();
}

CsiService::~CsiService() {
    while (true) {
        const bool runtimeResourcesPresent =
            (_processingTaskHandle != nullptr) ||
            (_queue != nullptr) ||
            (_cleanupSem != nullptr) ||
            (_rxCallbackRegistered.load(std::memory_order_acquire)) ||
            (_rxCallbackEnabled.load(std::memory_order_acquire));

        if (!_enabled.load(std::memory_order_acquire) && !runtimeResourcesPresent) {
            break;
        }

        // The destructor may run after a partial startup or partial shutdown.
        // Keep funneling everything through the normal disable path until the
        // callback, task, queue and cleanup semaphore are all gone.
        _activeConsumers.store(0, std::memory_order_relaxed);

        // Force disable path to release callback, task, queue and semaphores.
        if (!_enabled.load(std::memory_order_relaxed) && runtimeResourcesPresent) {
            _enabled.store(true, std::memory_order_relaxed);
        }

        if (applyEnabledState(false)) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (_callbackMutex) {
        vSemaphoreDelete(_callbackMutex);
        _callbackMutex = nullptr;
    }
    if (_motionCallbackMutex) {
        vSemaphoreDelete(_motionCallbackMutex);
        _motionCallbackMutex = nullptr;
    }
    if (_motionPublishMutex) {
        vSemaphoreDelete(_motionPublishMutex);
        _motionPublishMutex = nullptr;
    }
    if (_motionConfigMutex) {
        vSemaphoreDelete(_motionConfigMutex);
        _motionConfigMutex = nullptr;
    }
    if (_motionControlMutex) {
        vSemaphoreDelete(_motionControlMutex);
        _motionControlMutex = nullptr;
    }
    if (_stateMutex) {
        vSemaphoreDelete(_stateMutex);
        _stateMutex = nullptr;
    }
    _motionDetector.end();
}

void CsiService::begin() {
    if (!_motionDetector.begin()) {
        LOGE("Failed to allocate CSI motion detector buffers in PSRAM");
    }
    publishMotionSnapshot(_motionDetector.snapshot());
    resetVisualizationState();
    LOGI("Service initialized (Disabled by default)");
    // Queue is now allocated lazily when the first consumer becomes active.
}

void CsiService::setCsiCallback(CsiCallback cb) {
    if (!_callbackMutex) {
        _csiCallback = std::move(cb);
        return;
    }

    // Hold the ownership lock while the old callback drains. The worker
    // increments its in-flight count before releasing this same lock, so no
    // copied lambda can outlive a successful detach/replacement.
    SYSTEM::ScopeLock lock(_callbackMutex, portMAX_DELAY);
    if (!lock.isLocked()) {
        LOGE("CSI callback ownership mutex unavailable");
        return;
    }
    _csiCallback = nullptr;
    const uint32_t waitStartedMs = millis();
    bool timeoutLogged = false;
    while (_csiCallbacksInFlight.load(std::memory_order_acquire) != 0) {
        if (!timeoutLogged &&
            (millis() - waitStartedMs) >= TIMEOUT::TASK_SHUTDOWN_MS) {
            LOGW("Waiting for in-flight CSI data callback to drain");
            timeoutLogged = true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    _csiCallback = std::move(cb);
}

void CsiService::setMotionCallback(MotionCallback cb) {
    const bool callbackInstalled = static_cast<bool>(cb);
    SYSTEM::ScopeLock publishLock(_motionPublishMutex, portMAX_DELAY);
    if (!publishLock.isLocked()) {
        LOGE("CSI motion publication mutex unavailable");
        return;
    }

    if (!_motionCallbackMutex) {
        _motionCallback = cb;
    } else {
        SYSTEM::ScopeLock lock(_motionCallbackMutex, portMAX_DELAY);
        if (!lock.isLocked()) {
            LOGE("CSI motion callback mutex unavailable");
            return;
        }

        _motionCallback = cb;
    }

    // Callback ownership changed, so the new consumer has not seen any state.
    // This callback must not re-enter CsiService while the publication mutex is
    // held; the current alarm bridge only updates its own mailbox.
    _motionPublicationGate.invalidate();
    if (!callbackInstalled) {
        return;
    }

    // During boot the constructor default is disabled even when persisted
    // settings will enable CSI a few lines later. Publishing that temporary
    // default would clear an RTC-retained active alarm. The settings
    // transaction below owns the first definitive synchronization.
    if (_motionBootstrapGate.pending()) {
        return;
    }

    // The lifecycle/config transaction will publish its own terminal state.
    // Synchronizing from the previous config here could clear retained motion
    // while an enabled boot config is still being applied.
    if (_motionControlFence.transitionInProgress()) {
        return;
    }

    bool alarmMotionDesired = true;
    if (_motionConfigMutex && xSemaphoreTake(_motionConfigMutex, portMAX_DELAY) == pdTRUE) {
        alarmMotionDesired = _alarmMotionConfig.enabled;
        xSemaphoreGive(_motionConfigMutex);
    }
    if (!alarmMotionDesired) {
        bool value = false;
        (void)_motionSyncMailbox.takeForEpoch(
            _motionControlFence.requestedEpoch(), value);
        cb(value);
        _motionPublicationGate.markPublished(value, millis());
    } else {
        // A queued clear belongs to an older disabled epoch. It must never
        // clear a retained enabled alarm after CSI is enabled again.
        _motionSyncMailbox.discard();
        const CsiMotionSnapshot snapshot = getMotionSnapshot();
        const bool shouldSync =
            snapshot.state == CsiMotionState::Disabled || snapshot.decisionValid;
        if (shouldSync) {
            const bool motion = snapshot.state == CsiMotionState::Disabled
                                    ? false
                                    : snapshot.motion;
            cb(motion);
            _motionPublicationGate.markPublished(motion, millis());
        }
    }
}

bool CsiService::prepareMotionCallbackBootstrap(bool retainedMotion) {
    SYSTEM::ScopeLock controlLock(_motionControlMutex, portMAX_DELAY);
    SYSTEM::ScopeLock publishLock(_motionPublishMutex, portMAX_DELAY);
    if (!controlLock.isLocked() || !publishLock.isLocked()) {
        LOGE("Failed to prepare CSI motion callback bootstrap");
        return false;
    }

    // Latch retained ownership before persisted config is applied. The
    // detector itself cannot restore it yet because its current config is the
    // constructor-default disabled state; applyMotionConfigLocked() consumes
    // this flag after configuring the detector.
    _motionSyncMailbox.discard();
    _motionPublicationGate.invalidate();
    _motionBootstrapGate.begin(retainedMotion);
    return true;
}

bool CsiService::setMotionConfig(const CsiMotionConfig& config) {
    SYSTEM::ScopeLock controlLock(_motionControlMutex, portMAX_DELAY);
    if (!controlLock.isLocked()) {
        LOGW("Failed to serialize CSI motion config update");
        return false;
    }

    return applyMotionConfigLocked(config);
}

bool CsiService::applyMotionConfigLocked(const CsiMotionConfig& config) {
    if (config.enabled && _shuttingDown.load(std::memory_order_acquire)) {
        LOGW("Rejecting CSI motion enable during terminal shutdown");
        return false;
    }

    uint32_t commandEpoch = 0;
    bool detectorReady = false;
    {
        SYSTEM::ScopeLock publishLock(_motionPublishMutex, portMAX_DELAY);
        if (!publishLock.isLocked()) {
            LOGW("Failed to start CSI motion config transition");
            return false;
        }

        // Close publication first, then commit the new requested epoch. A
        // packet that captured the previous epoch can no longer publish either
        // its callback edge or its UI snapshot while lifecycle work is running.
        commandEpoch = _motionControlFence.beginTransition();
        _motionPublicationGate.invalidate();

        // begin() allocates detector storage during normal boot, but a
        // transient PSRAM failure must remain recoverable.  Retry while the
        // publication lock and transition fence exclude the CSI worker from
        // reading storage/snapshot state that begin() is resetting.
        detectorReady =
            !config.enabled || _motionDetector.storageReady() || _motionDetector.begin();
        if (config.enabled) {
            _motionSyncMailbox.discard();
        }
    }

    (void)setConsumerActive(CsiConsumer::AlarmSystem, config.enabled);
    bool alarmConsumerReachedDesiredState = false;
    bool runtimeReachedDesiredState = false;
    {
        // Snapshot lifecycle pointers under their owning mutex. Other CSI
        // consumers can transition concurrently, so raw queue/task reads here
        // would otherwise be a formal data race and could report false success.
        SYSTEM::ScopeLock stateLock(_stateMutex, portMAX_DELAY);
        if (stateLock.isLocked()) {
            const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
            const bool noConsumersRemain = mask == 0;
            alarmConsumerReachedDesiredState =
                ((mask & consumerBit(CsiConsumer::AlarmSystem)) != 0) == config.enabled;
            runtimeReachedDesiredState = config.enabled
                                             ? _enabled.load(std::memory_order_relaxed)
                                             : (!noConsumersRemain ||
                                                (!_enabled.load(std::memory_order_relaxed) &&
                                                 !hasRuntimeResources()));
        }
    }
    const bool consumerReachedDesiredState =
        detectorReady &&
        alarmConsumerReachedDesiredState &&
        runtimeReachedDesiredState;

    // Once lifecycle work has started, this transaction must reach a terminal
    // state. Returning on a diagnostic timeout would leave the fence closed and
    // the consumer/runtime partially changed.
    SYSTEM::ScopeLock publishLock(_motionPublishMutex, portMAX_DELAY);

    if (!consumerReachedDesiredState) {
        // A requested enable is still the authoritative desired state at boot.
        // Configure the detector even when transport startup failed so retained
        // motion can remain latched; publish Unavailable, never a false clear.
        if (config.enabled) {
            if (_motionConfigMutex) {
                SYSTEM::ScopeLock configLock(_motionConfigMutex, portMAX_DELAY);
                _alarmMotionConfig = config;
            } else {
                _alarmMotionConfig = config;
            }
            _motionDetector.configure(config);
            if (_motionBootstrapGate.retained()) {
                _motionDetector.restoreRetainedMotion(true);
            }
            _motionGainReady = false;
        }
        CsiMotionSnapshot unavailable = _motionDetector.snapshot();
        unavailable.state = CsiMotionState::Unavailable;
        unavailable.decisionValid = false;
        unavailable.noisy = false;
        unavailable.needsCalibration = false;
        publishMotionSnapshot(unavailable);
        _motionRuntimeFault.store(true, std::memory_order_release);
        (void)_motionControlFence.completeTransition(commandEpoch);
        _motionBootstrapGate.complete();
        LOGE("CSI alarm consumer failed to reach requested state");
        return false;
    }

    if (_motionConfigMutex) {
        SYSTEM::ScopeLock configLock(_motionConfigMutex, portMAX_DELAY);
        _alarmMotionConfig = config;
    } else {
        _alarmMotionConfig = config;
    }

    _motionDetector.configure(config);
    if (config.enabled && _motionBootstrapGate.retained()) {
        // begin() may have just recovered PSRAM and reset the detector to its
        // startup state. Re-apply the retained active latch before publishing
        // any auto-calibrating snapshot so recovery cannot manufacture a clear.
        _motionDetector.restoreRetainedMotion(true);
    }
    _motionGainReady = false;
    publishMotionSnapshot(_motionDetector.snapshot());
    _motionRuntimeFault.store(false, std::memory_order_release);
    (void)_motionControlFence.completeTransition(commandEpoch);
    if (!config.enabled) {
        _motionBootstrapGate.retain(false);
        publishMotionValueLocked(false, millis());
    }
    _motionBootstrapGate.complete();
    return true;
}

bool CsiService::requestMotionCalibration() {
    SYSTEM::ScopeLock controlLock(_motionControlMutex, pdMS_TO_TICKS(500));
    if (!controlLock.isLocked()) {
        LOGW("Failed to serialize CSI motion calibration");
        return false;
    }

    SYSTEM::ScopeLock publishLock(_motionPublishMutex, pdMS_TO_TICKS(500));
    if (!publishLock.isLocked()) {
        LOGW("Failed to serialize CSI motion calibration");
        return false;
    }

    CsiMotionConfig config;
    if (_motionConfigMutex) {
        SYSTEM::ScopeLock configLock(_motionConfigMutex, pdMS_TO_TICKS(200));
        if (!configLock.isLocked()) {
            return false;
        }
        config = _alarmMotionConfig;
    }
    const uint32_t nowMs = millis();
    const CsiMotionSnapshot snapshot = _motionDetector.snapshot();
    if (!CsiMotionCalibrationGate::canStart(
            config.enabled,
            config.bandCount,
            isEnabled(),
            _motionDetector.storageReady(),
            _gainCtrl.isForced(),
            _motionRuntimeFault.load(std::memory_order_acquire),
            snapshot,
            nowMs,
            CSI_MOTION_STALE_AFTER_MS)) {
        return false;
    }

    const uint32_t commandEpoch = _motionControlFence.beginTransition();
    _motionDetector.resetBaseline(CsiMotionResetReason::ManualCalibration);
    _motionPublicationGate.invalidate();
    publishMotionSnapshot(_motionDetector.snapshot());
    (void)_motionControlFence.completeTransition(commandEpoch);
    return true;
}

void CsiService::restoreRetainedMotion(bool motion) {
    if (!motion) {
        return;
    }
    SYSTEM::ScopeLock controlLock(_motionControlMutex, portMAX_DELAY);
    SYSTEM::ScopeLock publishLock(_motionPublishMutex, portMAX_DELAY);
    if (!controlLock.isLocked() || !publishLock.isLocked()) {
        LOGW("Failed to serialize retained CSI motion restore");
        return;
    }

    bool alarmMotionDesired = false;
    if (_motionConfigMutex && xSemaphoreTake(_motionConfigMutex, portMAX_DELAY) == pdTRUE) {
        alarmMotionDesired = _alarmMotionConfig.enabled;
        xSemaphoreGive(_motionConfigMutex);
    }
    if (!alarmMotionDesired) {
        return;
    }

    _motionBootstrapGate.retain(true);

    const uint32_t commandEpoch = _motionControlFence.beginTransition();
    _motionDetector.restoreRetainedMotion(true);
    _motionPublicationGate.invalidate();
    CsiMotionSnapshot snapshot = _motionDetector.snapshot();
    if (!isEnabled() || _motionRuntimeFault.load(std::memory_order_acquire)) {
        snapshot.state = CsiMotionState::Unavailable;
        snapshot.decisionValid = false;
        snapshot.noisy = false;
        snapshot.needsCalibration = false;
    }
    publishMotionSnapshot(snapshot);
    (void)_motionControlFence.completeTransition(commandEpoch);
}

void CsiService::requestVisualizationReset() {
    _visualizationResetRequested.store(true, std::memory_order_release);
}

CsiMotionSnapshot CsiService::getMotionSnapshot() const {
    portENTER_CRITICAL(&_motionSnapshotMux);
    const CsiMotionSnapshot copy = _lastMotionSnapshot;
    portEXIT_CRITICAL(&_motionSnapshotMux);
    return copy;
}

CsiVisualizationSnapshot CsiService::getVisualizationSnapshot() const {
    portENTER_CRITICAL(&_visualizationSnapshotMux);
    const CsiVisualizationSnapshot copy = _lastVisualizationSnapshot;
    portEXIT_CRITICAL(&_visualizationSnapshotMux);
    return copy;
}

void CsiService::resetRuntimeMetrics() {
    _rxFramesTotal.store(0, std::memory_order_relaxed);
    _rxAcceptedTotal.store(0, std::memory_order_relaxed);
    _rxThrottledTotal.store(0, std::memory_order_relaxed);
    _queuedPacketsTotal.store(0, std::memory_order_relaxed);
    _dequeuedPacketsTotal.store(0, std::memory_order_relaxed);
    _packetsForwardedTotal.store(0, std::memory_order_relaxed);
    _batchesForwardedTotal.store(0, std::memory_order_relaxed);
    _batchesDroppedTotal.store(0, std::memory_order_relaxed);
    _packetsPerSec.store(0, std::memory_order_relaxed);
    _batchesPerSec.store(0, std::memory_order_relaxed);
    _queueDropsLastSec.store(0, std::memory_order_relaxed);
    _lastPacketMs.store(0, std::memory_order_relaxed);
    _lastBatchMs.store(0, std::memory_order_relaxed);
    _lastPacketsRateTotal = 0;
    _lastBatchesRateTotal = 0;
    resetVisualizationState();
    if (_queue) {
        _queue->resetStats();
    }
}

CsiMetricsSnapshot CsiService::getMetricsSnapshot() const {
    CsiMetricsSnapshot snapshot;
    snapshot.enabled = isEnabled();
    snapshot.runtimeFault = _motionRuntimeFault.load(std::memory_order_acquire);

    const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
    snapshot.activeConsumerMask = mask;
    snapshot.activeConsumerCount = countActiveConsumers(mask);
    snapshot.frontendConsumerActive = (mask & consumerBit(CsiConsumer::Frontend)) != 0;
    snapshot.alarmConsumerActive = (mask & consumerBit(CsiConsumer::AlarmSystem)) != 0;
    snapshot.bootConsumerActive = (mask & consumerBit(CsiConsumer::Boot)) != 0;
    snapshot.matrixVisualizationConsumerActive = (mask & consumerBit(CsiConsumer::MatrixVisualization)) != 0;
    snapshot.diagnosticCaptureConsumerActive =
        (mask & consumerBit(CsiConsumer::DiagnosticCapture)) != 0;

    snapshot.rxFramesTotal = _rxFramesTotal.load(std::memory_order_relaxed);
    snapshot.rxAcceptedTotal = _rxAcceptedTotal.load(std::memory_order_relaxed);
    snapshot.rxThrottledTotal = _rxThrottledTotal.load(std::memory_order_relaxed);
    snapshot.queuedPacketsTotal = _queuedPacketsTotal.load(std::memory_order_relaxed);
    snapshot.dequeuedPacketsTotal = _dequeuedPacketsTotal.load(std::memory_order_relaxed);
    snapshot.packetsForwardedTotal = _packetsForwardedTotal.load(std::memory_order_relaxed);
    snapshot.batchesForwardedTotal = _batchesForwardedTotal.load(std::memory_order_relaxed);
    snapshot.batchesDroppedTotal = _batchesDroppedTotal.load(std::memory_order_relaxed);
    snapshot.packetsPerSec = _packetsPerSec.load(std::memory_order_relaxed);
    snapshot.batchesPerSec = _batchesPerSec.load(std::memory_order_relaxed);
    snapshot.queueDropsLastSec = _queueDropsLastSec.load(std::memory_order_relaxed);
    snapshot.lastPacketMs = _lastPacketMs.load(std::memory_order_relaxed);
    snapshot.lastBatchMs = _lastBatchMs.load(std::memory_order_relaxed);
    snapshot.motionControlEpoch = _motionControlFence.requestedEpoch();
    snapshot.calibrationCount = _gainCtrl.calibrationCount();
    snapshot.calibrationTarget = CsiGainController::CALIBRATION_PACKETS;
    snapshot.calibrationState = _gainCtrl.stateName();
    snapshot.motion = getMotionSnapshot();
    if (snapshot.motion.hasFrame) {
        snapshot.motionFrameAgeMs = millis() - snapshot.motion.lastFrameMs;
        snapshot.motionDataFresh = snapshot.motionFrameAgeMs <= CSI_MOTION_STALE_AFTER_MS;
    }
    snapshot.visualization = getVisualizationSnapshot();

    if (!_stateMutex) {
        return snapshot;
    }

    SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) {
        return snapshot;
    }

    snapshot.queueAllocated = _queue != nullptr;
    const uint32_t lockedMask = _activeConsumers.load(std::memory_order_relaxed);
    const bool desiredEnabled = lockedMask != 0;
    const bool alarmDesired =
        (lockedMask & consumerBit(CsiConsumer::AlarmSystem)) != 0;
    snapshot.runtimeReconcilePending =
        desiredEnabled != _enabled.load(std::memory_order_relaxed) ||
        (!desiredEnabled && hasRuntimeResources()) ||
        (alarmDesired && snapshot.runtimeFault);
    if (_queue) {
        snapshot.queueMetricsValid = true;
        snapshot.queueDepth = _queue->getDepth();
        snapshot.queueCapacity = _queue->getCapacity();
        snapshot.queueDropsTotal = _queue->getDroppedPacketsTotal();
    }

    return snapshot;
}

void CsiService::applyPendingVisualizationCommandsNonBlocking() {
    if (_visualizationResetRequested.exchange(false, std::memory_order_acq_rel)) {
        resetVisualizationState();
    }
}

CsiMotionSnapshot CsiService::processMotionPacket(CsiPacket& packet,
                                                  uint32_t nowMs,
                                                  uint32_t expectedControlEpoch) {
    SYSTEM::ScopeLock publishLock(_motionPublishMutex, pdMS_TO_TICKS(50));
    CsiMotionSnapshot snapshot = getMotionSnapshot();
    if (!publishLock.isLocked() ||
        _motionRuntimeFault.load(std::memory_order_acquire) ||
        !_motionControlFence.canPublish(expectedControlEpoch)) {
        packet.motionScore = snapshot.score;
        packet.isMotionDetected = snapshot.motion;
        return snapshot;
    }

    if (!_gainCtrl.isForced()) {
        snapshot = _motionDetector.snapshot();
        packet.motionScore = snapshot.score;
        packet.isMotionDetected = snapshot.motion;
        return snapshot;
    }
    if (!_motionGainReady) {
        _motionGainReady = true;
        _motionDetector.resetBaseline(CsiMotionResetReason::GainStabilized);
        _motionPublicationGate.invalidate();
    }
    snapshot = _motionDetector.process(packet, nowMs);
    if (snapshot.decisionValid) {
        _motionBootstrapGate.retain(snapshot.motion);
    }
    packet.motionScore = snapshot.score;
    packet.isMotionDetected = snapshot.motion;
    publishMotionSnapshot(snapshot);

    if (_motionPublicationGate.shouldPublish(snapshot, nowMs, MOTION_KEEPALIVE_MS)) {
        MotionCallback callback = getMotionCallbackSnapshot();
        if (callback) {
            callback(snapshot.motion);
            _motionPublicationGate.markPublished(snapshot.motion, nowMs);
        }
    }
    return snapshot;
}

void CsiService::markMotionDataUnavailableIfStale(uint32_t nowMs,
                                                  uint32_t expectedControlEpoch) {
    SYSTEM::ScopeLock publishLock(_motionPublishMutex, pdMS_TO_TICKS(50));
    if (!publishLock.isLocked() ||
        _motionRuntimeFault.load(std::memory_order_acquire) ||
        !_motionControlFence.canPublish(expectedControlEpoch)) {
        return;
    }

    const CsiMotionSnapshot currentMotion = _motionDetector.snapshot();
    if (currentMotion.hasFrame &&
        currentMotion.state != CsiMotionState::Unavailable &&
        (nowMs - currentMotion.lastFrameMs) > CSI_MOTION_STALE_AFTER_MS) {
        publishMotionSnapshot(
            _motionDetector.markDataUnavailable(CsiMotionResetReason::FrameGap));
    }
}

CsiVisualizationSnapshot CsiService::processVisualizationPacket(const CsiPacket& packet, uint32_t nowMs) {
    return _visualizationReducer.process(packet, nowMs);
}

bool CsiService::hasRuntimeResources() const {
    return (_processingTaskHandle != nullptr) ||
           (_queue != nullptr) ||
           (_cleanupSem != nullptr) ||
           (_rxCallbackRegistered.load(std::memory_order_acquire)) ||
           (_rxCallbackEnabled.load(std::memory_order_acquire));
}

void CsiService::publishMotionSnapshot(const CsiMotionSnapshot& snapshot) {
    portENTER_CRITICAL(&_motionSnapshotMux);
    _lastMotionSnapshot = snapshot;
    portEXIT_CRITICAL(&_motionSnapshotMux);
}

void CsiService::publishVisualizationSnapshot(const CsiVisualizationSnapshot& snapshot) {
    portENTER_CRITICAL(&_visualizationSnapshotMux);
    _lastVisualizationSnapshot = snapshot;
    portEXIT_CRITICAL(&_visualizationSnapshotMux);
}

void CsiService::publishMotionValueLocked(bool motion, uint32_t nowMs) {
    _motionSyncMailbox.queue(motion, _motionControlFence.requestedEpoch());
    if (!_motionPublicationGate.shouldPublishValue(motion, nowMs, MOTION_KEEPALIVE_MS)) {
        _motionSyncMailbox.discard();
        return;
    }

    MotionCallback callback = getMotionCallbackSnapshot();
    if (!callback) {
        // Keep a durable value for callback installation or a later retry. This
        // is required when disabling the last consumer leaves no worker alive.
        return;
    }
    callback(motion);
    _motionPublicationGate.markPublished(motion, nowMs);
    _motionSyncMailbox.discard();
}

void CsiService::recordBatchDelivery(size_t packetCount, bool accepted) {
    if (packetCount == 0) {
        return;
    }

    if (accepted) {
        _batchesForwardedTotal.fetch_add(1, std::memory_order_relaxed);
        _packetsForwardedTotal.fetch_add(static_cast<uint32_t>(packetCount), std::memory_order_relaxed);
        _lastBatchMs.store(millis(), std::memory_order_relaxed);
        return;
    }

    _batchesDroppedTotal.fetch_add(1, std::memory_order_relaxed);
}

void CsiService::resetVisualizationState() {
    _visualizationReducer.reset();
    publishVisualizationSnapshot(_visualizationReducer.snapshot());
}

uint32_t CsiService::consumerBit(CsiConsumer consumer) {
    return 1u << static_cast<uint8_t>(consumer);
}

bool CsiService::hasActiveConsumers() const {
    return _activeConsumers.load(std::memory_order_relaxed) != 0;
}

bool CsiService::isConsumerActive(CsiConsumer consumer) const {
    return (_activeConsumers.load(std::memory_order_relaxed) & consumerBit(consumer)) != 0;
}

bool CsiService::isRuntimeReady() const {
    if (!_stateMutex) {
        return false;
    }

    SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) {
        return false;
    }

    return isCsiRuntimeReadyState(
        _enabled.load(std::memory_order_relaxed),
        _queue != nullptr,
        _processingTaskHandle != nullptr,
        _shouldExit.load(std::memory_order_acquire),
        _ping.isActive(),
        _rxCallbackRegistered.load(std::memory_order_acquire),
        _rxCallbackEnabled.load(std::memory_order_acquire));
}

bool CsiService::setConsumerActive(CsiConsumer consumer, bool active) {
    if (active && _shuttingDown.load(std::memory_order_acquire)) {
        LOGW("Rejecting CSI consumer enable during terminal shutdown");
        return false;
    }

    const uint32_t bit = consumerBit(consumer);
    if (active) {
        _activeConsumers.fetch_or(bit, std::memory_order_acq_rel);
    } else {
        _activeConsumers.fetch_and(~bit, std::memory_order_acq_rel);
    }

    // Desired ownership is committed atomically before this bounded lock. If
    // another lifecycle transition holds _stateMutex for several seconds, the
    // request remains visible to the periodic reconciler instead of vanishing.
    SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(200));
    if (!lock.isLocked()) {
        LOGW("Failed to update CSI consumer state");
        return false;
    }

    const uint32_t nextMask = _activeConsumers.load(std::memory_order_acquire);
    const bool desiredEnabled = (nextMask != 0);

    // Service lifetime follows the aggregate desired bitmask, not whichever
    // caller toggled last. That way the frontend, boot path and alarm
    // integration cannot accidentally disable CSI for one another.
    const bool currentEnabled = _enabled.load(std::memory_order_relaxed);
    if (desiredEnabled != currentEnabled || (!desiredEnabled && hasRuntimeResources())) {
        if (!applyEnabledState(desiredEnabled)) {
            return false;
        }
    }

    return _enabled.load(std::memory_order_relaxed) == desiredEnabled &&
           (desiredEnabled || !hasRuntimeResources());
}

bool CsiService::needsRuntimeReconcile() {
    if (_shuttingDown.load(std::memory_order_acquire)) {
        return false;
    }
    SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) {
        return true;
    }

    const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
    const bool desiredEnabled = mask != 0;
    const bool runtimeEnabled = _enabled.load(std::memory_order_relaxed);
    if (desiredEnabled != runtimeEnabled || (!desiredEnabled && hasRuntimeResources())) {
        return true;
    }

    const bool alarmDesired =
        (mask & consumerBit(CsiConsumer::AlarmSystem)) != 0;
    return alarmDesired && _motionRuntimeFault.load(std::memory_order_acquire);
}

bool CsiService::reconcileRuntime() {
    if (_shuttingDown.load(std::memory_order_acquire)) {
        return false;
    }

    bool reconcileMotion = false;
    bool desiredEnabled = false;
    {
        SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(200));
        if (!lock.isLocked()) {
            return false;
        }

        const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
        desiredEnabled = mask != 0;
        reconcileMotion =
            (mask & consumerBit(CsiConsumer::AlarmSystem)) != 0 &&
            _motionRuntimeFault.load(std::memory_order_acquire);

        if (!reconcileMotion) {
            const bool runtimeEnabled = _enabled.load(std::memory_order_relaxed);
            if (desiredEnabled == runtimeEnabled &&
                (desiredEnabled || !hasRuntimeResources())) {
                return true;
            }
            if (!applyEnabledState(desiredEnabled)) {
                return false;
            }
            return _enabled.load(std::memory_order_relaxed) == desiredEnabled &&
                   (desiredEnabled || !hasRuntimeResources());
        }
    }

    // Serialize the config copy and repair under the same control-plane lock.
    // Otherwise a concurrent HTTP update could apply config B between copying
    // old A and calling setMotionConfig(A), letting the reconciler overwrite B.
    SYSTEM::ScopeLock controlLock(_motionControlMutex, portMAX_DELAY);
    if (!controlLock.isLocked()) {
        return false;
    }

    CsiMotionConfig config;
    {
        SYSTEM::ScopeLock configLock(_motionConfigMutex, pdMS_TO_TICKS(200));
        if (!configLock.isLocked()) {
            return false;
        }
        config = _alarmMotionConfig;
    }
    return applyMotionConfigLocked(config);
}

bool CsiService::shutdown() {
    _shuttingDown.store(true, std::memory_order_release);
    SYSTEM::ScopeLock lock(_stateMutex, portMAX_DELAY);
    if (!lock.isLocked()) {
        return false;
    }

    _activeConsumers.store(0, std::memory_order_relaxed);
    return applyEnabledState(false);
}

bool CsiService::applyEnabledState(bool enabled) {
    if (enabled && _shuttingDown.load(std::memory_order_acquire)) {
        return false;
    }
    const bool runtimeResourcesPresent = hasRuntimeResources();

    if (_enabled.load(std::memory_order_relaxed) == enabled) {
        if (!enabled && runtimeResourcesPresent) {
            // Keep going so a partially stopped runtime can be reaped.
        } else {
            return true;
        }
    }

    if (enabled) {
        // A previous shutdown/rollback may intentionally retain callback-backed
        // resources after a detach or drain failure. Finish that teardown before
        // opening the callback gate for a new runtime generation.
        if (!_enabled.load(std::memory_order_relaxed) && runtimeResourcesPresent) {
            if (!applyEnabledState(false)) {
                LOGW("Previous CSI runtime cleanup is still pending");
                return false;
            }
        }

        LOGI("Enabling CSI Sensing (Allocating Resources)...");
        bool csiConfigured = false;
        
        // The queue must exist before the driver callback is registered, otherwise
        // the first CSI frame could arrive while the handoff path is still null.
        if (!_queue) {
            _queue = new CsiDataQueue(CSI_QUEUE_CAPACITY);
            if (!_queue->begin()) {
                LOGE("Failed to allocate CSI Queue!");
                delete _queue;
                _queue = nullptr;
                return false;
            }
        }

        // 1b. Create Cleanup Semaphore
        if (!_cleanupSem) {
            _cleanupSem = xSemaphoreCreateBinary();
        }
        if (!_cleanupSem) {
            LOGE("Failed to allocate CSI cleanup semaphore");
            rollbackFailedEnable(false);
            return false;
        }

        // Reset Components
        _gainCtrl.reset();
        _motionGainReady = false;
        resetRuntimeMetrics();
        
        // Adaptive Rate Reset
        _rxFrameCount.store(0, std::memory_order_relaxed);
        _lastRateCheckTime = millis();
        _currentPingInterval = SENSOR::WIFI_SENSING::CSI_PING_INTERVAL_MS;
        _lastRxAcceptTimeUs.store(0, std::memory_order_relaxed);
        // Open the software gate before registering with the Wi-Fi driver. During
        // shutdown we close it, detach the stable callback owner, then drain
        // leases held in the process-lifetime callback fence.
        _rxCallbackEnabled.store(true, std::memory_order_release);

        if (!initCsiConfig()) {
            rollbackFailedEnable(false);
            return false;
        }
        csiConfigured = true;
        // Ping traffic is the deliberate producer for repeatable CSI samples. Without
        // it, an idle network can leave the callback with little or no input to process.
        _ping.start();
        if (!_ping.isActive()) {
            LOGE("CSI ping producer failed to start, rolling back enable path");
            rollbackFailedEnable(csiConfigured);
            return false;
        }
        if (!startProcessingTask()) {
            LOGE("CSI consumer task failed to start, rolling back enable path");
            rollbackFailedEnable(csiConfigured);
            return false;
        }

        // Publish "enabled" only after queue, callback, ping and processing task are alive.
        _enabled.store(true, std::memory_order_relaxed);
        return true;
    } else {
        LOGI("Disabling CSI Sensing (Freeing Resources)...");
        _enabled.store(false, std::memory_order_relaxed);

        // Teardown order is deliberate:
        // 1. stop new callback entries and wait for in-flight copies to finish
        // 2. stop the producer side (driver + ping)
        // 3. stop the consumer task
        // 4. destroy the queue and cleanup semaphore
        //
        // Reordering this can turn a clean shutdown into a race with the ISR path.
        _rxCallbackEnabled.store(false, std::memory_order_release);
        // Close the stable driver context before unregistering. Even a callback
        // already dispatched but not yet entered will now observe no owner.
        detachRxCallbackOwner();
        bool callbackDetached =
            !_rxCallbackRegistered.load(std::memory_order_acquire);
        if (!callbackDetached) {
            const esp_err_t detachErr = esp_wifi_set_csi_rx_cb(nullptr, nullptr);
            if (detachErr == ESP_OK) {
                _rxCallbackRegistered.store(false, std::memory_order_release);
                callbackDetached = true;
            } else {
                LOGW("Failed to detach CSI RX callback: %s", esp_err_to_name(detachErr));
            }
        }

        bool callbacksDrained = false;
        if (callbackDetached) {
            callbacksDrained = waitForRxCallbacksToDrain(TIMEOUT::TASK_SHUTDOWN_MS);
        }
        if (callbackDetached && !callbacksDrained) {
            LOGW("Timed out waiting for in-flight CSI callbacks to drain");
        }

        // 2. Stop the driver producer after the callback path is detached.
        const esp_err_t disableErr = esp_wifi_set_csi(false);
        if (disableErr != ESP_OK) {
            LOGW("Failed to disable CSI driver: %s", esp_err_to_name(disableErr));
        }
        // 3. Stop the worker before closing its ping socket or deleting the
        // queue it reads from. The worker is the sole caller of _ping.send().
        const bool processingTaskStopped = stopProcessingTask();
        if (!processingTaskStopped) {
            LOGW("CSI processing task did not suspend cleanly - keeping queue/resources allocated");
        } else {
            _ping.stop();
        }

        if (!canReleaseCsiRuntimeResources(
                callbackDetached, callbacksDrained, processingTaskStopped)) {
            LOGW("CSI runtime cleanup deferred until callback detach/drain succeeds");
            return false;
        }
        
        // 4. Free queue only after the RX callback path is guaranteed inactive.
        if (_queue) {
            delete _queue;
            _queue = nullptr;
        }

        if (_cleanupSem) {
            vSemaphoreDelete(_cleanupSem);
            _cleanupSem = nullptr;
        }

        resetVisualizationState();
        return true;
    }

    return true;
}

bool CsiService::initCsiConfig() {
    // Keep the common LTF variants enabled so CSI remains available across the
    // PHY modes we see from normal AP traffic. Channel filtering stays on to
    // reduce obviously noisy data before the app-level pipeline sees it.
    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = true, 
        .manu_scale        = false,
        .shift             = false,
    };
    const esp_err_t configErr = esp_wifi_set_csi_config(&csi_config);
    if (configErr != ESP_OK) {
        LOGE("Failed to configure CSI: %s", esp_err_to_name(configErr));
        return false;
    }

    if (!attachRxCallbackOwner()) {
        LOGE("Failed to claim stable CSI RX callback context");
        return false;
    }

    const esp_err_t callbackErr =
        esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, rxCallbackContext());
    if (callbackErr != ESP_OK) {
        detachRxCallbackOwner();
        LOGE("Failed to register CSI RX callback: %s", esp_err_to_name(callbackErr));
        return false;
    }
    _rxCallbackRegistered.store(true, std::memory_order_release);

    const esp_err_t enableErr = esp_wifi_set_csi(true);
    if (enableErr != ESP_OK) {
        LOGE("Failed to enable CSI driver: %s", esp_err_to_name(enableErr));
        // rollbackFailedEnable() owns detach/drain so a failed detach cannot be
        // mistaken for a safe queue teardown.
        return false;
    }

    LOGI("CSI Configured and Active");
    return true;
}

void CsiService::rollbackFailedEnable(bool csiConfigured) {
    // Undo partial startup in reverse dependency order so no producer or task can
    // outlive the queue / callback path it expects to use.
    _rxCallbackEnabled.store(false, std::memory_order_release);
    detachRxCallbackOwner();

    const bool callbackWasRegistered =
        _rxCallbackRegistered.load(std::memory_order_acquire);
    bool callbackDetached = !callbackWasRegistered;
    if (callbackWasRegistered) {
        const esp_err_t detachErr = esp_wifi_set_csi_rx_cb(nullptr, nullptr);
        if (detachErr == ESP_OK) {
            _rxCallbackRegistered.store(false, std::memory_order_release);
            callbackDetached = true;
        } else {
            LOGW("Rollback: failed to detach CSI RX callback: %s", esp_err_to_name(detachErr));
        }
    }

    bool callbacksDrained = false;
    if (callbackDetached) {
        callbacksDrained = waitForRxCallbacksToDrain(TIMEOUT::TASK_SHUTDOWN_MS);
    }
    if (callbackDetached && !callbacksDrained) {
        LOGW("Rollback: timed out waiting for CSI callbacks to drain");
    }

    if (csiConfigured || callbackWasRegistered) {
        const esp_err_t disableErr = esp_wifi_set_csi(false);
        if (disableErr != ESP_OK) {
            LOGW("Rollback: failed to disable CSI driver: %s", esp_err_to_name(disableErr));
        }
    }

    const bool processingTaskStopped = stopProcessingTask();
    _enabled.store(false, std::memory_order_relaxed);
    if (!processingTaskStopped) {
        LOGW("Rollback: CSI processing task still stopping, deferring queue cleanup");
    } else {
        _ping.stop();
    }

    if (!canReleaseCsiRuntimeResources(
            callbackDetached, callbacksDrained, processingTaskStopped)) {
        LOGW("Rollback: retaining CSI queue/semaphore for a later cleanup retry");
        return;
    }

    if (_queue) {
        delete _queue;
        _queue = nullptr;
    }

    if (_cleanupSem) {
        vSemaphoreDelete(_cleanupSem);
        _cleanupSem = nullptr;
    }

    resetVisualizationState();
}

} // namespace CSI
} // namespace WIFISENSING
