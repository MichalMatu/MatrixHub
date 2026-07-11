#include <Arduino.h>

#include "CsiService.h"
#include "CsiRxCallbackFence.h"

#include <cstring>
#include <esp_timer.h>

#include "../../../system/logging/Logging.h"
#include "../../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "CsiService"

namespace WIFISENSING {
namespace CSI {
namespace {

// The context deliberately has process lifetime. The Espressif API does not
// document unregister as a drain barrier, so a driver-dispatched callback may
// begin after the owning CsiService has already closed its entry gate.
CsiRxCallbackFence g_rxCallbackFence;

} // namespace

void IRAM_ATTR CsiService::wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    auto* fence = static_cast<CsiRxCallbackFence*>(ctx);
    // This runs on the Wi-Fi driver's task hot path. Keep it copy-only: no heap
    // allocation, no locking, no logging, no PSRAM handoff and no heavy signal
    // processing here.
    if (!fence) return;

    // Acquire against a stable process-lifetime context before loading the
    // service pointer. A callback delayed before this first instruction can no
    // longer dereference a destroyed CsiService after detach.
    auto* self = static_cast<CsiService*>(fence->enter());
    if (!self) {
        fence->leave();
        return;
    }

    if (!self->_rxCallbackEnabled.load(std::memory_order_acquire) ||
        !info || !info->buf) {
        fence->leave();
        return;
    }

    // Keep one stable pointer for the whole protected callback. The queue cannot
    // be released until this callback decrements the in-flight counter.
    CsiDataQueue* const queue = self->_queue;
    if (!queue) {
        fence->leave();
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
        fence->leave();
        return;
    }
    self->_lastRxAcceptTimeUs.store(nowUs, std::memory_order_relaxed);
    const uint32_t acceptedSequence =
        self->_rxAcceptedTotal.fetch_add(1, std::memory_order_relaxed) + 1;

    CsiPacket packet;
    memcpy(&packet.rx_ctrl, &info->rx_ctrl, sizeof(wifi_pkt_rx_ctrl_t));
    memcpy(packet.mac, info->mac, 6);
    memcpy(packet.dmac, info->dmac, 6);
    packet.originalLen = info->len;
    packet.len = (info->len > MAX_CSI_DATA_LEN) ? MAX_CSI_DATA_LEN : info->len;
    packet.rxSequence = info->rx_seq;
    packet.firstWordInvalid = info->first_word_invalid;
    packet.acceptedSequence = acceptedSequence;
    memcpy(packet.buf, info->buf, packet.len);
    // Worker task computes the real compensation after dequeue, once we're out of ISR context.
    packet.compensate_gain = 1.0f;

    // Queue tracks overflow statistics internally; the callback stays branch-light either way.
    if (queue->pushFromWifiTask(packet)) {
        self->_queuedPacketsTotal.fetch_add(1, std::memory_order_relaxed);
    }
    fence->leave();
}

bool CsiService::attachRxCallbackOwner() {
    return g_rxCallbackFence.attach(this);
}

void CsiService::detachRxCallbackOwner() {
    g_rxCallbackFence.detach(this);
}

void* CsiService::rxCallbackContext() {
    return &g_rxCallbackFence;
}

CsiCallback CsiService::getCsiCallbackSnapshot() {
    if (!_callbackMutex) {
        return nullptr;
    }

    SYSTEM::ScopeLock lock(_callbackMutex, pdMS_TO_TICKS(20));
    if (!lock.isLocked()) {
        return nullptr;
    }

    CsiCallback callback = _csiCallback;
    if (callback) {
        _csiCallbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
    }
    return callback;
}

void CsiService::releaseCsiCallbackSnapshot() {
    _csiCallbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
}

MotionCallback CsiService::getMotionCallbackSnapshot() {
    if (!_motionCallbackMutex) {
        return nullptr;
    }

    if (xSemaphoreTake(_motionCallbackMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
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
    while (g_rxCallbackFence.hasInFlight()) {
        if ((millis() - startMs) >= timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

} // namespace CSI
} // namespace WIFISENSING
