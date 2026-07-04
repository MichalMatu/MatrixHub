#include "HttpServerHealthTracker.h"

#include "../../logging/Logging.h"

#include <algorithm>
#if !defined(ARDUINO_ARCH_ESP32)
#include <mutex>
#endif

#undef LOG_TAG
#define LOG_TAG "HttpHealth"

namespace SYSTEM {
namespace HEALTH {

namespace {

#if defined(ARDUINO_ARCH_ESP32)
portMUX_TYPE sHttpHealthMux = portMUX_INITIALIZER_UNLOCKED;

class HttpHealthGuard {
public:
    HttpHealthGuard() {
        portENTER_CRITICAL(&sHttpHealthMux);
    }

    ~HttpHealthGuard() {
        portEXIT_CRITICAL(&sHttpHealthMux);
    }
};
#else
std::mutex sHttpHealthMutex;

class HttpHealthGuard {
public:
    HttpHealthGuard() {
        sHttpHealthMutex.lock();
    }

    ~HttpHealthGuard() {
        sHttpHealthMutex.unlock();
    }
};
#endif

}  // namespace

HttpServerHealthSnapshot HttpServerHealthTracker::_snapshot;
uint32_t HttpServerHealthTracker::_recentCloseWindowStartMs = 0;
uint8_t HttpServerHealthTracker::_recentCloseBurstCount = 0;

void HttpServerHealthTracker::reset() {
    HttpHealthGuard guard;
    _snapshot = {};
    _recentCloseWindowStartMs = 0;
    _recentCloseBurstCount = 0;
}

void HttpServerHealthTracker::recordOpen() {
    bool peakIncreased = false;
    HttpServerHealthSnapshot snapshotAfterUpdate;
    {
        HttpHealthGuard guard;
        const uint32_t now = millis();

        // This is a transport-level open. The expected steady-state invariant is:
        // activeClients == opens - closes for real sockets accepted by the
        // server. WebSocket upgrades must not call into this path.
        _snapshot.openCount++;
        _snapshot.lastOpenMs = now;
        _snapshot.activeClients++;

        if (_snapshot.activeClients > _snapshot.peakClients) {
            _snapshot.peakClients = _snapshot.activeClients;
            peakIncreased = true;
        }

        snapshotAfterUpdate = _snapshot;
    }

    if (peakIncreased) {
        LOGD("HTTP client peak increased: active=%lu peak=%lu opens=%lu closes=%lu",
             static_cast<unsigned long>(snapshotAfterUpdate.activeClients),
             static_cast<unsigned long>(snapshotAfterUpdate.peakClients),
             static_cast<unsigned long>(snapshotAfterUpdate.openCount),
             static_cast<unsigned long>(snapshotAfterUpdate.closeCount));
    }
}

void HttpServerHealthTracker::recordClose() {
    bool churnSpikeDetected = false;
    uint8_t closeBurstCount = 0;
    HttpServerHealthSnapshot snapshotAfterUpdate;
    {
        HttpHealthGuard guard;
        const uint32_t now = millis();

        // Closes are paired with the framework onClose callback. We guard the
        // decrement so late cleanup or duplicate close notifications do not
        // underflow the diagnostic counter during stressful reconnect storms.
        _snapshot.closeCount++;
        _snapshot.lastCloseMs = now;
        if (_snapshot.activeClients > 0) {
            _snapshot.activeClients--;
        }

        if (_recentCloseWindowStartMs == 0 || (now - _recentCloseWindowStartMs) > 30000UL) {
            _recentCloseWindowStartMs = now;
            _recentCloseBurstCount = 1;
            return;
        }

        _recentCloseBurstCount = std::min<uint8_t>(static_cast<uint8_t>(_recentCloseBurstCount + 1), UINT8_MAX);
        if (_recentCloseBurstCount == 8) {
            churnSpikeDetected = true;
            closeBurstCount = _recentCloseBurstCount;
        }

        snapshotAfterUpdate = _snapshot;
    }

    if (churnSpikeDetected) {
        LOGW("HTTP client churn spike detected: closes=%u in 30s (active=%lu forced_ws=%lu)",
             static_cast<unsigned>(closeBurstCount),
             static_cast<unsigned long>(snapshotAfterUpdate.activeClients),
             static_cast<unsigned long>(snapshotAfterUpdate.wsForcedRemovals));
    }
}

void HttpServerHealthTracker::recordWsOpen() {
    HttpHealthGuard guard;
    const uint32_t now = millis();
    _snapshot.wsOpenCount++;
    _snapshot.lastWsOpenMs = now;
    _snapshot.wsActiveClients++;
    if (_snapshot.wsActiveClients > _snapshot.wsPeakClients) {
        _snapshot.wsPeakClients = _snapshot.wsActiveClients;
    }
}

void HttpServerHealthTracker::recordWsClose() {
    HttpHealthGuard guard;
    const uint32_t now = millis();
    _snapshot.wsCloseCount++;
    _snapshot.lastWsCloseMs = now;
    if (_snapshot.wsActiveClients > 0) {
        _snapshot.wsActiveClients--;
    }
}

void HttpServerHealthTracker::recordWsForcedRemoval(int fd) {
    uint32_t totalForcedRemovals = 0;
    {
        HttpHealthGuard guard;
        _snapshot.wsForcedRemovals++;
        totalForcedRemovals = _snapshot.wsForcedRemovals;
    }

    LOGW("Forced WebSocket client removal: fd=%d total=%lu",
         fd,
         static_cast<unsigned long>(totalForcedRemovals));
}

void HttpServerHealthTracker::recordWsQueueDrop(size_t payloadLen) {
    HttpHealthGuard guard;
    _snapshot.wsQueueDrops++;
    _snapshot.lastWsQueueDropMs = millis();
    _snapshot.lastWsQueueDropPayload = static_cast<uint32_t>(payloadLen);
}

void HttpServerHealthTracker::recordWsHeapFallback(size_t payloadLen) {
    HttpHealthGuard guard;
    _snapshot.wsHeapFallbacks++;
    _snapshot.lastWsHeapFallbackMs = millis();
    _snapshot.lastWsHeapFallbackPayload = static_cast<uint32_t>(payloadLen);
    if (payloadLen > _snapshot.maxWsHeapFallbackPayload) {
        _snapshot.maxWsHeapFallbackPayload = static_cast<uint32_t>(payloadLen);
    }
}

HttpServerHealthSnapshot HttpServerHealthTracker::getSnapshot() {
    HttpHealthGuard guard;
    return _snapshot;
}

}  // namespace HEALTH
}  // namespace SYSTEM
