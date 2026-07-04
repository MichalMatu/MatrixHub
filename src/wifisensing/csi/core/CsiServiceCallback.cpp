#include <Arduino.h>

#include "CsiService.h"

#include <cstring>
#include <esp_timer.h>

#include "../../../system/logging/Logging.h"
#include "../../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "CsiService"

namespace WIFISENSING {
namespace CSI {

void IRAM_ATTR CsiService::wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    CsiService* self = (CsiService*)ctx;
    // This runs on the Wi-Fi driver's hot path. Keep it copy-only: no heap
    // allocation, no locking, no logging and no heavy signal processing here.
    if (!info || !info->buf || !self || !self->_queue) return;
    if (!self->_rxCallbackEnabled.load(std::memory_order_acquire)) return;

    // Count the callback before doing work so shutdown can wait for any instance
    // that already passed the first gate. The second gate closes the race where
    // disable happens just after we increment the in-flight counter.
    self->_rxCallbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
    if (!self->_rxCallbackEnabled.load(std::memory_order_acquire)) {
        self->_rxCallbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    // Rx Counting for Adaptive Rate
    self->_rxFrameCount.fetch_add(1, std::memory_order_relaxed);
    self->_rxFramesTotal.fetch_add(1, std::memory_order_relaxed);

    // Early drops here are intentional backpressure. It is better to reject a
    // frame before the queue than to let short bursts hide the true source of loss.
    // Accept up to ~16.6 packets per second (60ms interval) to ensure stable 10Hz
    // output despite FreeRTOS scheduling jitter and ping intervals (-20 to +20ms).
    uint32_t nowUs = (uint32_t)esp_timer_get_time();
    uint32_t lastUs = self->_lastRxAcceptTimeUs.load(std::memory_order_relaxed);
    uint32_t elapsedUs = nowUs - lastUs;
    if (elapsedUs < CSI_RX_THROTTLE_INTERVAL_US) {
        self->_rxThrottledTotal.fetch_add(1, std::memory_order_relaxed);
        self->_rxCallbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
    self->_lastRxAcceptTimeUs.store(nowUs, std::memory_order_relaxed);
    self->_rxAcceptedTotal.fetch_add(1, std::memory_order_relaxed);

    CsiPacket packet;
    memcpy(&packet.rx_ctrl, &info->rx_ctrl, sizeof(wifi_pkt_rx_ctrl_t));
    memcpy(packet.mac, info->mac, 6);
    packet.len = (info->len > MAX_CSI_DATA_LEN) ? MAX_CSI_DATA_LEN : info->len;
    memcpy(packet.buf, info->buf, packet.len);
    // Worker task computes the real compensation after dequeue, once we're out of ISR context.
    packet.compensate_gain = 1.0f;

    // Queue tracks overflow statistics internally; the callback stays branch-light either way.
    if (self->_queue->pushFromIsr(packet)) {
        self->_queuedPacketsTotal.fetch_add(1, std::memory_order_relaxed);
    }
    self->_rxCallbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
}

CsiCallback CsiService::getCsiCallbackSnapshot() {
    if (!_callbackMutex) {
        return nullptr;
    }

    SYSTEM::ScopeLock lock(_callbackMutex, pdMS_TO_TICKS(20));
    if (!lock.isLocked()) {
        return nullptr;
    }

    return _csiCallback;
}

MotionCallback CsiService::getMotionCallbackSnapshot() {
    if (!_motionCallbackMutex) {
        return nullptr;
    }

    if (xSemaphoreTake(_motionCallbackMutex, 0) != pdTRUE) {
        return nullptr;
    }

    MotionCallback callback = _motionCallback;
    xSemaphoreGive(_motionCallbackMutex);
    return callback;
}

bool CsiService::waitForRxCallbacksToDrain(uint32_t timeoutMs) {
    const uint32_t startMs = millis();
    // Called only after callback registration has been removed, so the counter
    // represents work already in flight rather than new callback arrivals.
    while (_rxCallbacksInFlight.load(std::memory_order_acquire) != 0) {
        if ((millis() - startMs) >= timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

} // namespace CSI
} // namespace WIFISENSING
