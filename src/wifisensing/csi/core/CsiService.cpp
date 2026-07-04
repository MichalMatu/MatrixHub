#include <Arduino.h>
#include "CsiService.h"
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <new>
#include "../../../system/logging/Logging.h" 
#include "../../../system/utils/ScopeLock.h"
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
    _motionConfigMutex = xSemaphoreCreateMutex();
}

CsiService::~CsiService() {
    while (true) {
        const bool runtimeResourcesPresent =
            (_processingTaskHandle != nullptr) ||
            (_queue != nullptr) ||
            (_cleanupSem != nullptr) ||
            (_rxCallbackEnabled.load(std::memory_order_acquire)) ||
            (_rxCallbacksInFlight.load(std::memory_order_acquire) != 0);

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
    if (_motionConfigMutex) {
        vSemaphoreDelete(_motionConfigMutex);
        _motionConfigMutex = nullptr;
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

    SYSTEM::ScopeLock lock(_callbackMutex, pdMS_TO_TICKS(200));
    if (!lock.isLocked()) {
        LOGW("Failed to update CSI callback");
        return;
    }

    _csiCallback = std::move(cb);
}

void CsiService::setMotionCallback(MotionCallback cb) {
    if (!_motionCallbackMutex) {
        _motionCallback = std::move(cb);
        return;
    }

    SYSTEM::ScopeLock lock(_motionCallbackMutex, pdMS_TO_TICKS(200));
    if (!lock.isLocked()) {
        LOGW("Failed to update CSI motion callback");
        return;
    }

    _motionCallback = std::move(cb);
}

void CsiService::setMotionConfig(const CsiMotionConfig& config) {
    if (_motionConfigMutex) {
        SYSTEM::ScopeLock lock(_motionConfigMutex, pdMS_TO_TICKS(200));
        if (!lock.isLocked()) {
            LOGW("Failed to update CSI motion config");
            return;
        }
        _alarmMotionConfig = config;
    } else {
        _alarmMotionConfig = config;
    }

    setConsumerActive(CsiConsumer::AlarmSystem, config.enabled);
    refreshMotionConfigFromConsumers();
}

void CsiService::requestMotionCalibration() {
    _motionCalibrationRequested.store(true, std::memory_order_release);
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

    const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
    snapshot.activeConsumerMask = mask;
    snapshot.activeConsumerCount = countActiveConsumers(mask);
    snapshot.frontendConsumerActive = (mask & consumerBit(CsiConsumer::Frontend)) != 0;
    snapshot.alarmConsumerActive = (mask & consumerBit(CsiConsumer::AlarmSystem)) != 0;
    snapshot.bootConsumerActive = (mask & consumerBit(CsiConsumer::Boot)) != 0;
    snapshot.matrixVisualizationConsumerActive = (mask & consumerBit(CsiConsumer::MatrixVisualization)) != 0;

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
    snapshot.calibrationCount = _gainCtrl.calibrationCount();
    snapshot.calibrationTarget = CsiGainController::CALIBRATION_PACKETS;
    snapshot.calibrationState = _gainCtrl.stateName();
    snapshot.motion = getMotionSnapshot();
    snapshot.visualization = getVisualizationSnapshot();

    if (!_stateMutex) {
        return snapshot;
    }

    SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(50));
    if (!lock.isLocked()) {
        return snapshot;
    }

    snapshot.queueAllocated = _queue != nullptr;
    if (_queue) {
        snapshot.queueDepth = _queue->getDepth();
        snapshot.queueCapacity = _queue->getCapacity();
        snapshot.queueDropsTotal = _queue->getDroppedPacketsTotal();
    }

    return snapshot;
}

void CsiService::applyPendingMotionCommandsNonBlocking() {
    if (_motionConfigDirty.load(std::memory_order_acquire)) {
        if (_motionConfigMutex && xSemaphoreTake(_motionConfigMutex, 0) == pdTRUE) {
            const CsiMotionConfig config = _pendingMotionConfig;
            _motionConfigDirty.store(false, std::memory_order_release);
            xSemaphoreGive(_motionConfigMutex);
            _motionDetector.configure(config);
            publishMotionSnapshot(_motionDetector.snapshot());
            if (!config.enabled) {
                publishMotionBoolean(false, millis());
            }
        }
    }

    if (_motionCalibrationRequested.exchange(false, std::memory_order_acq_rel)) {
        _motionDetector.resetBaseline();
        publishMotionSnapshot(_motionDetector.snapshot());
    }
}

CsiMotionSnapshot CsiService::processMotionPacket(CsiPacket& packet, uint32_t nowMs) {
    CsiMotionSnapshot snapshot = _motionDetector.process(packet, nowMs);
    packet.motionScore = snapshot.score;
    packet.isMotionDetected = snapshot.motion;
    return snapshot;
}

CsiVisualizationSnapshot CsiService::processVisualizationPacket(const CsiPacket& packet, uint32_t nowMs) {
    return _visualizationReducer.process(packet, nowMs);
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

void CsiService::maybePublishMotion(const CsiMotionSnapshot& snapshot, uint32_t nowMs) {
    const bool changed = snapshot.motion != _lastPublishedMotion;
    const bool keepalive =
        snapshot.motion && (nowMs - _lastMotionCallbackMs >= MOTION_KEEPALIVE_MS);

    if (changed || keepalive) {
        publishMotionBoolean(snapshot.motion, nowMs);
    }
}

void CsiService::publishMotionBoolean(bool motion, uint32_t nowMs) {
    const bool changed = motion != _lastPublishedMotion;
    const bool keepalive = motion && (nowMs - _lastMotionCallbackMs >= MOTION_KEEPALIVE_MS);
    if (!changed && !keepalive) {
        return;
    }

    MotionCallback callback = getMotionCallbackSnapshot();
    _lastPublishedMotion = motion;
    _lastMotionCallbackMs = nowMs;
    if (callback) {
        callback(motion);
    }
}

void CsiService::refreshMotionConfigFromConsumers() {
    CsiMotionConfig selectedConfig;

    if (_motionConfigMutex) {
        SYSTEM::ScopeLock lock(_motionConfigMutex, pdMS_TO_TICKS(200));
        if (!lock.isLocked()) {
            LOGW("Failed to refresh CSI motion config");
            return;
        }

        const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
        const bool alarmActive =
            (mask & consumerBit(CsiConsumer::AlarmSystem)) != 0 && _alarmMotionConfig.enabled;

        if (alarmActive) {
            selectedConfig = _alarmMotionConfig;
        } else {
            selectedConfig.enabled = false;
        }

        _pendingMotionConfig = selectedConfig;
        _motionConfigDirty.store(true, std::memory_order_release);
    } else {
        const uint32_t mask = _activeConsumers.load(std::memory_order_relaxed);
        const bool alarmActive =
            (mask & consumerBit(CsiConsumer::AlarmSystem)) != 0 && _alarmMotionConfig.enabled;

        if (alarmActive) {
            selectedConfig = _alarmMotionConfig;
        } else {
            selectedConfig.enabled = false;
        }

        _pendingMotionConfig = selectedConfig;
        _motionConfigDirty.store(true, std::memory_order_release);
    }

    if (!selectedConfig.enabled) {
        CsiMotionSnapshot disabled;
        disabled.state = CsiMotionState::Disabled;
        publishMotionSnapshot(disabled);
        publishMotionBoolean(false, millis());
        _motionCalibrationRequested.store(false, std::memory_order_release);
    }
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

bool CsiService::setConsumerActive(CsiConsumer consumer, bool active) {
    SYSTEM::ScopeLock lock(_stateMutex, pdMS_TO_TICKS(200));
    if (!lock.isLocked()) {
        LOGW("Failed to update CSI consumer state");
        return _enabled.load(std::memory_order_relaxed);
    }

    const uint32_t bit = consumerBit(consumer);
    const uint32_t previousMask = _activeConsumers.load(std::memory_order_relaxed);
    uint32_t nextMask = previousMask;

    if (active) {
        nextMask |= bit;
    } else {
        nextMask &= ~bit;
    }

    if (nextMask == previousMask) {
        if ((nextMask != 0) != _enabled.load(std::memory_order_relaxed)) {
            applyEnabledState(nextMask != 0);
        }
        return _enabled.load(std::memory_order_relaxed);
    }

    // Service lifetime follows the aggregate consumer bitmask, not whichever
    // caller toggled last. That way the frontend, boot path and future alarm
    // integrations cannot accidentally disable CSI for one another.
    const bool desiredEnabled = (nextMask != 0);
    const bool currentEnabled = _enabled.load(std::memory_order_relaxed);
    if (desiredEnabled != currentEnabled) {
        if (!applyEnabledState(desiredEnabled)) {
            return _enabled.load(std::memory_order_relaxed);
        }
    }

    _activeConsumers.store(nextMask, std::memory_order_relaxed);
    return _enabled.load(std::memory_order_relaxed);
}

bool CsiService::applyEnabledState(bool enabled) {
    const bool runtimeResourcesPresent =
        (_processingTaskHandle != nullptr) ||
        (_queue != nullptr) ||
        (_cleanupSem != nullptr) ||
        (_rxCallbackEnabled.load(std::memory_order_acquire)) ||
        (_rxCallbacksInFlight.load(std::memory_order_acquire) != 0);

    if (_enabled.load(std::memory_order_relaxed) == enabled) {
        if (!enabled && runtimeResourcesPresent) {
            // Keep going so a partially stopped runtime can be reaped.
        } else {
            return true;
        }
    }

    if (enabled) {
        LOGI("Enabling CSI Sensing (Allocating Resources)...");
        bool csiConfigured = false;
        
        // The queue must exist before the driver callback is registered, otherwise
        // the first CSI frame could arrive while the handoff path is still null.
        if (!_queue) {
            _queue = new CsiDataQueue(16); // 16 packets buffer (~1.6s at 10Hz throttled)
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
        resetRuntimeMetrics();
        
        // Adaptive Rate Reset
        _rxFrameCount.store(0, std::memory_order_relaxed);
        _lastRateCheckTime = millis();
        _currentPingInterval = SENSOR::WIFI_SENSING::CSI_PING_INTERVAL_MS;
        _lastRxAcceptTimeUs.store(0, std::memory_order_relaxed);
        _rxCallbacksInFlight.store(0, std::memory_order_relaxed);
        // Open the software gate before registering with the Wi-Fi driver. During
        // shutdown we flip this gate first, then wait for _rxCallbacksInFlight == 0.
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
        const esp_err_t detachErr = esp_wifi_set_csi_rx_cb(nullptr, nullptr);
        if (detachErr != ESP_OK) {
            LOGW("Failed to detach CSI RX callback: %s", esp_err_to_name(detachErr));
        }
        if (!waitForRxCallbacksToDrain(TIMEOUT::TASK_SHUTDOWN_MS)) {
            LOGW("Timed out waiting for in-flight CSI callbacks to drain");
        }

        // 2. Stop producers after the callback path is detached.
        const esp_err_t disableErr = esp_wifi_set_csi(false);
        if (disableErr != ESP_OK) {
            LOGW("Failed to disable CSI driver: %s", esp_err_to_name(disableErr));
        }
        _ping.stop();

        // 3. Stop consumer before deleting the queue it reads from.
        if (!stopProcessingTask()) {
            LOGW("CSI processing task did not suspend cleanly - keeping queue/resources allocated");
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

    const esp_err_t callbackErr = esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, this);
    if (callbackErr != ESP_OK) {
        LOGE("Failed to register CSI RX callback: %s", esp_err_to_name(callbackErr));
        return false;
    }

    const esp_err_t enableErr = esp_wifi_set_csi(true);
    if (enableErr != ESP_OK) {
        LOGE("Failed to enable CSI driver: %s", esp_err_to_name(enableErr));
        esp_wifi_set_csi_rx_cb(nullptr, nullptr);
        return false;
    }

    LOGI("CSI Configured and Active");
    return true;
}

void CsiService::rollbackFailedEnable(bool csiConfigured) {
    // Undo partial startup in reverse dependency order so no producer or task can
    // outlive the queue / callback path it expects to use.
    _rxCallbackEnabled.store(false, std::memory_order_release);

    if (csiConfigured) {
        const esp_err_t detachErr = esp_wifi_set_csi_rx_cb(nullptr, nullptr);
        if (detachErr != ESP_OK) {
            LOGW("Rollback: failed to detach CSI RX callback: %s", esp_err_to_name(detachErr));
        }
        if (!waitForRxCallbacksToDrain(TIMEOUT::TASK_SHUTDOWN_MS)) {
            LOGW("Rollback: timed out waiting for CSI callbacks to drain");
        }

        const esp_err_t disableErr = esp_wifi_set_csi(false);
        if (disableErr != ESP_OK) {
            LOGW("Rollback: failed to disable CSI driver: %s", esp_err_to_name(disableErr));
        }
    }

    _ping.stop();
    if (!stopProcessingTask()) {
        LOGW("Rollback: CSI processing task still stopping, deferring queue cleanup");
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
    _enabled.store(false, std::memory_order_relaxed);
}

} // namespace CSI
} // namespace WIFISENSING
