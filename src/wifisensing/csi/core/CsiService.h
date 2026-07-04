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

#include <functional>

namespace WIFISENSING {
namespace CSI {

// Callbacks defined in CsiTypes.h

enum class CsiConsumer : uint8_t {
    Frontend = 0,
    AlarmSystem = 1,
    Boot = 2,
    MatrixVisualization = 3,
};

struct CsiMetricsSnapshot {
    bool enabled = false;
    bool queueAllocated = false;
    uint32_t activeConsumerMask = 0;
    uint8_t activeConsumerCount = 0;
    bool frontendConsumerActive = false;
    bool alarmConsumerActive = false;
    bool bootConsumerActive = false;
    bool matrixVisualizationConsumerActive = false;
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
    bool hasActiveConsumers() const;
    CsiMetricsSnapshot getMetricsSnapshot() const;
    void recordBatchDelivery(size_t packetCount, bool accepted);

    // Register a callback to receive processed CSI packets (for API streaming)
    void setCsiCallback(CsiCallback cb);
    void setMotionCallback(MotionCallback cb);
    void setMotionConfig(const CsiMotionConfig& config);
    void requestMotionCalibration();
    CsiMotionSnapshot getMotionSnapshot() const;
    CsiVisualizationSnapshot getVisualizationSnapshot() const;

    // Components
    CsiPingSession _ping;
    CsiGainController _gainCtrl;

    // State
    std::atomic<bool> _enabled{false};
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

    // CSI Callback (Static for C-API compatibility)
    static void IRAM_ATTR wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info);
    
    // Internal initialization helper
    bool initCsiConfig();
    bool applyEnabledState(bool enabled);
    static uint32_t consumerBit(CsiConsumer consumer);
    bool waitForRxCallbacksToDrain(uint32_t timeoutMs);
    void rollbackFailedEnable(bool csiConfigured);
    CsiCallback getCsiCallbackSnapshot();
    MotionCallback getMotionCallbackSnapshot();
    void applyPendingMotionCommandsNonBlocking();
    CsiMotionSnapshot processMotionPacket(CsiPacket& packet, uint32_t nowMs);
    CsiVisualizationSnapshot processVisualizationPacket(const CsiPacket& packet, uint32_t nowMs);
    void publishMotionSnapshot(const CsiMotionSnapshot& snapshot);
    void publishVisualizationSnapshot(const CsiVisualizationSnapshot& snapshot);
    void maybePublishMotion(const CsiMotionSnapshot& snapshot, uint32_t nowMs);
    void publishMotionBoolean(bool motion, uint32_t nowMs);
    void refreshMotionConfigFromConsumers();
    void resetVisualizationState();
    bool reapStoppedProcessingTask(TickType_t waitTicks);
    void destroyProcessingTaskResources();
    void resetRuntimeMetrics();

    CsiDataQueue* _queue = nullptr;
    TaskHandle_t _processingTaskHandle = nullptr;
    SemaphoreHandle_t _cleanupSem = nullptr; // Sync for safe shutdown
    SemaphoreHandle_t _stateMutex = nullptr;
    SemaphoreHandle_t _callbackMutex = nullptr;
    SemaphoreHandle_t _motionCallbackMutex = nullptr;
    SemaphoreHandle_t _motionConfigMutex = nullptr;

    std::atomic<uint32_t> _activeConsumers{0};

    // Static Task Storage
    StackType_t* _taskStack = nullptr;
    StaticTask_t* _taskBuffer = nullptr;
    std::atomic<bool> _rxCallbackEnabled{false};
    // Shutdown waits for this counter after detaching the callback so the queue can
    // be destroyed without racing a callback that already started copying data.
    std::atomic<uint32_t> _rxCallbacksInFlight{0};

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
    CsiMotionConfig _pendingMotionConfig;
    std::atomic<bool> _motionConfigDirty{false};
    std::atomic<bool> _motionCalibrationRequested{false};
    mutable portMUX_TYPE _motionSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
    CsiMotionSnapshot _lastMotionSnapshot;
    mutable portMUX_TYPE _visualizationSnapshotMux = portMUX_INITIALIZER_UNLOCKED;
    CsiVisualizationSnapshot _lastVisualizationSnapshot;
    uint32_t _lastMotionCallbackMs = 0;
    bool _lastPublishedMotion = false;
    static constexpr uint32_t MOTION_KEEPALIVE_MS = 3000;
    
    CsiPacket* _batchBuffer = nullptr;
    static constexpr size_t BATCH_CAPACITY = MAX_CSI_BATCH_PACKETS;

};

} // namespace CSI
} // namespace WIFISENSING
