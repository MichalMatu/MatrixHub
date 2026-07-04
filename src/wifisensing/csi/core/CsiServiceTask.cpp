#include <Arduino.h>

#include "CsiService.h"

#include <esp_heap_caps.h>

#include "../../../system/logging/Logging.h"
#include "../../../system/utils/Random.h"

#include "config/System.h"

#undef LOG_TAG
#define LOG_TAG "CsiService"

namespace {

bool waitForTaskSuspended(TaskHandle_t taskHandle, SemaphoreHandle_t cleanupSem, TickType_t waitTicks) {
    // The task cleans up cooperatively, signals the semaphore, then suspends
    // itself. The parent task remains the owner that finally deletes the task
    // object and frees the static buffers.
    if (taskHandle == nullptr) {
        return true;
    }

    if (eTaskGetState(taskHandle) == eSuspended) {
        return true;
    }

    if (cleanupSem != nullptr) {
        if (xSemaphoreTake(cleanupSem, waitTicks) != pdTRUE &&
            eTaskGetState(taskHandle) != eSuspended) {
            return false;
        }
    } else if (waitTicks > 0) {
        return false;
    }

    const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    const TickType_t settleWait = pdMS_TO_TICKS(50);
    TickType_t waited = 0;

    while (eTaskGetState(taskHandle) != eSuspended && waited < settleWait) {
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return eTaskGetState(taskHandle) == eSuspended;
}

} // namespace

namespace WIFISENSING {
namespace CSI {

bool CsiService::startProcessingTask() {
    if (_processingTaskHandle) {
        if (!_shouldExit.load(std::memory_order_acquire)) {
            return true;
        }
        if (!reapStoppedProcessingTask(0)) {
            LOGW("Processing task is still stopping");
            return false;
        }
    }

    if (!_batchBuffer) {
        _batchBuffer = (CsiPacket*)heap_caps_malloc(BATCH_CAPACITY * sizeof(CsiPacket), MALLOC_CAP_SPIRAM);
    }

    // Batch buffer can live in PSRAM, but the task stack/control block stay in
    // internal RAM because this task drives networking-facing code paths.
    // Allocate stack in internal DRAM because this task drives network-facing code paths.
    uint32_t stackSize = CONFIG::TASKS::STACK_WIFI_SENSING_CSI;
    if (!_taskStack) {
        _taskStack = (StackType_t*)heap_caps_malloc(
            stackSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!_taskBuffer) {
        _taskBuffer = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!_taskStack || !_taskBuffer || !_batchBuffer) {
        LOGE("Failed to allocate memory for Processing Task!");
        destroyProcessingTaskResources();
        return false;
    }

    if (_cleanupSem) {
        (void)xSemaphoreTake(_cleanupSem, 0);
    }

    _processingTaskHandle = xTaskCreateStaticPinnedToCore(
        processingTask,
        "CsiProcess",
        stackSize, // In ESP-IDF this is bytes
        this,
        CONFIG::TASKS::PRIO_WIFI_SENSING,
        _taskStack,
        _taskBuffer,
        CONFIG::TASKS::CORE_WIFI_SENSING
    );

    if (!_processingTaskHandle) {
         LOGE("Failed to create Static Task!");
         destroyProcessingTaskResources();
         return false;
    }

    return true;
}

bool CsiService::stopProcessingTask() {
    if (!_processingTaskHandle) {
        destroyProcessingTaskResources();
        return true;
    }

    LOGI("Stopping Processing Task (Graceful)...");
    // The worker owns its exit point, but the service owns the task object's
    // lifetime. We therefore ask it to stop, wait for its cleanup handshake,
    // then delete it from the outside.
    const bool alreadyStopping = _shouldExit.exchange(true, std::memory_order_acq_rel);
    const TickType_t waitTicks = alreadyStopping ? 0 : pdMS_TO_TICKS(200);

    if (!reapStoppedProcessingTask(waitTicks)) {
        if (!alreadyStopping) {
            LOGW("Processing task cleanup timeout - keeping resources allocated");
        }
        return false;
    }

    LOGI("Task Resources Freed");
    return true;
}

bool CsiService::reapStoppedProcessingTask(TickType_t waitTicks) {
    if (!_processingTaskHandle) {
        destroyProcessingTaskResources();
        return true;
    }

    if (!_shouldExit.load(std::memory_order_acquire)) {
        return false;
    }

    if (!waitForTaskSuspended(_processingTaskHandle, _cleanupSem, waitTicks)) {
        return false;
    }

    vTaskDelete(_processingTaskHandle);
    destroyProcessingTaskResources();
    return true;
}

void CsiService::destroyProcessingTaskResources() {
    _processingTaskHandle = nullptr;
    _shouldExit.store(false, std::memory_order_release);

    // Only free these after the task is confirmed stopped. They back the static
    // FreeRTOS task object and the transient batch used by the worker loop.
    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }
    if (_batchBuffer) {
        heap_caps_free(_batchBuffer);
        _batchBuffer = nullptr;
    }
}

void CsiService::processingTask(void* param) {
    CsiService* self = (CsiService*)param;
    CsiPacket packet;

    LOGI("Csi Processing: Batching Enabled (Size %d, Timeout 500ms)", BATCH_CAPACITY);

    if (!self->_batchBuffer) {
        LOGE("Batch Buffer not allocated! Aborting Task.");
        vTaskSuspend(NULL);
        return;
    }

    size_t batchCount = 0;
    self->_shouldExit.store(false, std::memory_order_release);
    LOG_STACK_BUDGET(CONFIG::TASKS::STACK_BUDGET_WIFI_SENSING_CSI);

    // Adaptive ping state
    uint32_t nextInterval = SENSOR::WIFI_SENSING::CSI_PING_INTERVAL_MS;

    while (!self->_shouldExit.load(std::memory_order_acquire)) {
        uint32_t now = millis();
        self->applyPendingMotionCommandsNonBlocking();

        // Wait briefly for the first packet so the idle path is cheap.
        if (self->_queue && self->_queue->pop(packet, pdMS_TO_TICKS(10))) {
            self->_dequeuedPacketsTotal.fetch_add(1, std::memory_order_relaxed);
            self->_lastPacketMs.store(now, std::memory_order_relaxed);
            self->applyPendingMotionCommandsNonBlocking();
            packet.compensate_gain = self->_gainCtrl.update(&(packet.rx_ctrl));
            self->publishVisualizationSnapshot(self->processVisualizationPacket(packet, now));
            const auto motion = self->processMotionPacket(packet, now);
            self->publishMotionSnapshot(motion);
            self->maybePublishMotion(motion, now);
            self->_batchBuffer[batchCount++] = packet;

            // Once awake, drain the rest of the burst without blocking so multiple
            // queued frames can be forwarded in one callback invocation.
            while (batchCount < BATCH_CAPACITY && self->_queue->pop(packet, 0)) {
                self->_dequeuedPacketsTotal.fetch_add(1, std::memory_order_relaxed);
                self->_lastPacketMs.store(now, std::memory_order_relaxed);
                self->applyPendingMotionCommandsNonBlocking();
                packet.compensate_gain = self->_gainCtrl.update(&(packet.rx_ctrl));
                self->publishVisualizationSnapshot(self->processVisualizationPacket(packet, now));
                const auto motion = self->processMotionPacket(packet, now);
                self->publishMotionSnapshot(motion);
                self->maybePublishMotion(motion, now);
                self->_batchBuffer[batchCount++] = packet;
            }
        }

        now = millis();

        // Statistics are surfaced for debugging queue pressure. The current code
        // resets counters here but does not auto-tune ping cadence from them yet.
        if (now - self->_lastRateCheckTime >= 1000) {
             uint32_t droppedPackets = 0;
             if (self->_queue) {
                 droppedPackets = self->_queue->takeDroppedPackets();
                 if (droppedPackets > 0) {
                     LOGW("CSI ISR queue dropped %u packets in the last interval", droppedPackets);
                 }
             }
             self->_queueDropsLastSec.store(droppedPackets, std::memory_order_relaxed);
             const uint32_t forwardedPackets = self->_packetsForwardedTotal.load(std::memory_order_relaxed);
             const uint32_t forwardedBatches = self->_batchesForwardedTotal.load(std::memory_order_relaxed);
             self->_packetsPerSec.store(forwardedPackets - self->_lastPacketsRateTotal, std::memory_order_relaxed);
             self->_batchesPerSec.store(forwardedBatches - self->_lastBatchesRateTotal, std::memory_order_relaxed);
             self->_lastPacketsRateTotal = forwardedPackets;
             self->_lastBatchesRateTotal = forwardedBatches;
             self->_rxFrameCount.store(0, std::memory_order_relaxed);
             self->_lastRateCheckTime = now;
             self->_currentPingInterval = SENSOR::WIFI_SENSING::CSI_PING_INTERVAL_MS;
        }

        // Small jitter keeps the producer from phase-locking to task scheduling,
        // which would otherwise create deceptively regular sampling gaps.
        if (self->_enabled.load(std::memory_order_relaxed) && (now - self->_lastPingTime >= nextInterval)) {
             self->_ping.send();
             self->_lastPingTime = now;
             int32_t jitter = UTILS::RNG::rangeI32(-20, 19);
             nextInterval = self->_currentPingInterval + jitter;
        }

        // Flush every loop iteration. This keeps latency low while still amortizing
        // callback overhead when several frames were already queued together.
        if (batchCount > 0) {
            CsiCallback callback = self->getCsiCallbackSnapshot();
            if (callback) {
                callback(self->_batchBuffer, batchCount);
            } else {
                self->recordBatchDelivery(batchCount, false);
            }
            batchCount = 0;
        }

        LOG_STACK_BUDGET_PERIODIC(CONFIG::TASKS::STACK_BUDGET_WIFI_SENSING_CSI);
    }

    LOGI("Processing Task Exiting (Clean)");

    if (self->_cleanupSem) {
        xSemaphoreGive(self->_cleanupSem);
    }

    // Parent will call vTaskDelete on us after cleanup so task ownership stays in one place.
    vTaskSuspend(nullptr);
}

} // namespace CSI
} // namespace WIFISENSING
