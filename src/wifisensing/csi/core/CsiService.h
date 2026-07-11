#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <vector>
#include <atomic>
#include <esp_attr.h>

#include <esp_wifi_types.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/icmp.h> // Ensure standard ICMP header availability
#include "config/System.h"
#include "../data/CsiTypes.h"
#include "../data/CsiDataQueue.h"
#include "CsiPingSession.h"
#include "../algo/CsiGainController.h"
#include "../algo/CsiBandMotionDetector.h"
#include "../algo/CsiVisualizationReducer.h"
#include "CsiMotionPublicationGate.h"
#include "CsiMotionControlFence.h"
#include "CsiMotionBootstrapGate.h"

#include <functional>

namespace WIFISENSING {
namespace CSI {

// Callbacks defined in CsiTypes.h

enum class CsiConsumer : uint8_t {
    Frontend = 0,
    AlarmSystem = 1,
    Boot = 2,
    MatrixVisualization = 3,
    DiagnosticCapture = 4,
};

struct CsiMetricsSnapshot {
    bool enabled = false;
    bool runtimeFault = false;
    bool runtimeReconcilePending = false;
    bool queueAllocated = false;
    bool queueMetricsValid = false;
    uint32_t activeConsumerMask = 0;
    uint8_t activeConsumerCount = 0;
    bool frontendConsumerActive = false;
    bool alarmConsumerActive = false;
    bool bootConsumerActive = false;
    bool matrixVisualizationConsumerActive = false;
    bool diagnosticCaptureConsumerActive = false;
    size_t queueDepth = 0;
    size_t queueCapacity = 0;
    uint32_t queueDropsTotal = 0;
    uint32_t queueDropsLastSec = 0;
    uint32_t rxFramesTotal = 0;
    uint32_t rxAcceptedTotal = 0;
    uint32_t rxThrottledTotal = 0;
    uint32_t queuedPacketsTotal = 0;
    uint32_t dequeuedPacketsTotal = 0;
    uint32_t packetsForwardedTotal = 0;
    uint32_t batchesForwardedTotal = 0;
    uint32_t batchesDroppedTotal = 0;
    uint32_t packetsPerSec = 0;
    uint32_t batchesPerSec = 0;
    uint32_t lastPacketMs = 0;
    uint32_t lastBatchMs = 0;
    uint32_t motionControlEpoch = 0;
    bool motionDataFresh = false;
    uint32_t motionFrameAgeMs = 0;
    int calibrationCount = 0;
    int calibrationTarget = CsiGainController::CALIBRATION_PACKETS;
    const char* calibrationState = "unknown";
    CsiMotionSnapshot motion;
    CsiVisualizationSnapshot visualization;
};

class CsiService {
public:
    CsiService();
    ~CsiService();

    void begin();
    bool isEnabled() const { return _enabled.load(std::memory_order_relaxed); }
    bool setConsumerActive(CsiConsumer consumer, bool active);
    bool isConsumerActive(CsiConsumer consumer) const;
    bool isRuntimeReady() const;
    bool hasActiveConsumers() const;
    bool needsRuntimeReconcile();
    bool reconcileRuntime();
    bool shutdown();
    bool isProcessingTaskContext() const;
    CsiMetricsSnapshot getMetricsSnapshot() const;
    void recordBatchDelivery(size_t packetCount, bool accepted);

    // Register a callback to receive processed CSI packets (for API streaming)
    void setCsiCallback(CsiCallback cb);
    void setMotionCallback(MotionCallback cb);
    bool prepareMotionCallbackBootstrap(bool retainedMotion);
    bool setMotionConfig(const CsiMotionConfig& config);
    bool requestMotionCalibration();
    void restoreRetainedMotion(bool motion);
    /**
     * @brief Request a CSI matrix visualization reducer reset.
     *
     * The reducer is owned by the CSI processing task while packets are flowing.
     * HTTP/API callers use this non-blocking request so the reset is applied by
     * the worker before the next packet is processed.
     */
    void requestVisualizationReset();
    CsiMotionSnapshot getMotionSnapshot() const;
    CsiVisualizationSnapshot getVisualizationSnapshot() const;

    // Components
    CsiPingSession _ping;
    CsiGainController _gainCtrl;

    // State
    std::atomic<bool> _enabled{false};
    std::atomic<bool> _shuttingDown{false};
    std::atomic<bool> _shouldExit{false}; // Graceful shutdown flag
    uint32_t _lastPingTime = 0;

    CsiCallback _csiCallback = nullptr;
    MotionCallback _motionCallback = nullptr;

    // Processing Task
    bool startProcessingTask();
    bool stopProcessingTask();
    static void processingTask(void* param);

    // Rate Control
    std::atomic<uint32_t> _rxFrameCount{0};
    uint32_t _lastRateCheckTime = 0;
    uint32_t _currentPingInterval = 0;
    std::atomic<uint32_t> _lastRxAcceptTimeUs{0}; // Shared with ISR to enforce pre-queue throttling.

    // RX throttle (60ms = 60,000us) (safe for 10Hz ping with +-20ms jitter)
    static constexpr uint32_t CSI_RX_THROTTLE_INTERVAL_US = 60000;

    // Helpers
    // Ping session now managed via _ping class

    // CSI Callback (Static for C-API compatibility). Espressif runs this from
    // the Wi-Fi task, so it must never block or touch PSRAM-backed handoff data.
    static void IRAM_ATTR wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info);
    
    // Internal initialization helper
    bool initCsiConfig();
    bool applyMotionConfigLocked(const CsiMotionConfig& config);
    bool applyEnabledState(bool enabled);
    bool hasRuntimeResources() const;
    static uint32_t consumerBit(CsiConsumer consumer);
    bool waitForRxCallbacksToDrain(uint32_t timeoutMs);
    bool attachRxCallbackOwner();
    void detachRxCallbackOwner();
    static void* rxCallbackContext();
    void rollbackFailedEnable(bool csiConfigured);
    CsiCallback getCsiCallbackSnapshot();
    void releaseCsiCallbackSnapshot();
    MotionCallback getMotionCallbackSnapshot();
    void applyPendingVisualizationCommandsNonBlocking();
    CsiMotionSnapshot processMotionPacket(CsiPacket& packet,
                                          uint32_t nowMs,
                                          uint32_t expectedControlEpoch);
    void markMotionDataUnavailableIfStale(uint32_t nowMs,
                                          uint32_t expectedControlEpoch);
    CsiVisualizationSnapshot processVisualizationPacket(const CsiPacket& packet, uint32_t nowMs);
    void publishMotionSnapshot(const CsiMotionSnapshot& snapshot);
    void publishVisualizationSnapshot(const CsiVisualizationSnapshot& snapshot);
    void publishMotionValueLocked(bool motion, uint32_t nowMs);
    bool reapStoppedProcessingTask(TickType_t waitTicks);
    void destroyProcessingTaskResources();
    void resetRuntimeMetrics();
    void resetVisualizationState();

    CsiDataQueue* _queue = nullptr;
    TaskHandle_t _processingTaskHandle = nullptr;
    SemaphoreHandle_t _cleanupSem = nullptr; // Sync for safe shutdown
    SemaphoreHandle_t _stateMutex = nullptr;
    SemaphoreHandle_t _callbackMutex = nullptr;
    SemaphoreHandle_t _motionCallbackMutex = nullptr;
    SemaphoreHandle_t _motionPublishMutex = nullptr;
    SemaphoreHandle_t _motionConfigMutex = nullptr;
    SemaphoreHandle_t _motionControlMutex = nullptr;

    // Desired consumer ownership. It is committed before lifecycle work so a
    // transient start/stop failure remains visible and can be reconciled later.
    std::atomic<uint32_t> _activeConsumers{0};

    // Static Task Storage
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskBuffer = nullptr;
    std::atomic<bool> _rxCallbackEnabled{false};
    // Tracks driver ownership separately from the software entry gate. A failed
    // detach must keep callback-backed resources alive for a later retry.
    std::atomic<bool> _rxCallbackRegistered{false};
    // Protects the higher-level processed-batch callback captured by API
    // services. Callback replacement waits for this count before returning.
    std::atomic<uint32_t> _csiCallbacksInFlight{0};

    std::atomic<uint32_t> _rxFramesTotal{0};
    std::atomic<uint32_t> _rxAcceptedTotal{0};
    std::atomic<uint32_t> _rxThrottledTotal{0};
    std::atomic<uint32_t> _queuedPacketsTotal{0};
    std::atomic<uint32_t> _dequeuedPacketsTotal{0};
    std::atomic<uint32_t> _packetsForwardedTotal{0};
    std::atomic<uint32_t> _batchesForwardedTotal{0};
    std::atomic<uint32_t> _batchesDroppedTotal{0};
    std::atomic<uint32_t> _packetsPerSec{0};
    std::atomic<uint32_t> _batchesPerSec{0};
    std::atomic<uint32_t> _queueDropsLastSec{0};
    std::atomic<uint32_t> _lastPacketMs{0};
    std::atomic<uint32_t> _lastBatchMs{0};
    uint32_t _lastPacketsRateTotal = 0;
    uint32_t _lastBatchesRateTotal = 0;

    CsiBandMotionDetector _motionDetector;
    CsiVisualizationReducer _visualizationReducer;
    CsiMotionConfig _alarmMotionConfig;
    CsiMotionControlFence _motionControlFence;
    CsiMotionBootstrapGate _motionBootstrapGate;
    std::atomic<bool> _motionRuntimeFault{false};
    // The bootstrap gate also mirrors the last definitive/RTC-restored active
    // decision across detector storage recovery. A successful manual
    // calibration may clear it only after fresh definitive evidence.
    bool _motionGainReady = false;
    CsiMotionSyncMailbox _motionSyncMailbox;
    std::atomic<bool> _visualizationResetRequested{false};
    mutable portMUX_TYPE _motionSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
    CsiMotionSnapshot _lastMotionSnapshot;
    mutable portMUX_TYPE _visualizationSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
    CsiVisualizationSnapshot _lastVisualizationSnapshot;
    CsiMotionPublicationGate _motionPublicationGate;
    static constexpr uint32_t MOTION_KEEPALIVE_MS = 3000;
    
    CsiPacket* _batchBuffer = nullptr;
    static constexpr size_t BATCH_CAPACITY = MAX_CSI_BATCH_PACKETS;
    static constexpr size_t CSI_QUEUE_CAPACITY = 8;

};

} // namespace CSI
} // namespace WIFISENSING
