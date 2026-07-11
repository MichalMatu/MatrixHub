#pragma once

#include <atomic>
#include <stdint.h>

#include "../algo/CsiMotionTypes.h"

namespace WIFISENSING {
namespace CSI {

// Thread-safe publication policy shared by the CSI worker and the callback
// wiring path. Unknown detector states never emit a binary clear edge, and a
// newly installed consumer always receives the first definitive value.
class CsiMotionPublicationGate {
public:
    bool shouldPublish(const CsiMotionSnapshot& snapshot,
                       uint32_t nowMs,
                       uint32_t keepaliveMs) const {
        return snapshot.decisionValid &&
               shouldPublishValue(snapshot.motion, nowMs, keepaliveMs);
    }

    bool shouldPublishValue(bool motion,
                            uint32_t nowMs,
                            uint32_t keepaliveMs) const {
        if (!_hasPublished.load(std::memory_order_acquire)) {
            return true;
        }
        if (motion != _lastMotion.load(std::memory_order_relaxed)) {
            return true;
        }
        return motion &&
               (nowMs - _lastPublishedMs.load(std::memory_order_relaxed) >= keepaliveMs);
    }

    void markPublished(bool motion, uint32_t nowMs) {
        _lastMotion.store(motion, std::memory_order_relaxed);
        _lastPublishedMs.store(nowMs, std::memory_order_relaxed);
        _hasPublished.store(true, std::memory_order_release);
    }

    void invalidate() {
        _hasPublished.store(false, std::memory_order_release);
    }

    bool hasPublished() const {
        return _hasPublished.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> _hasPublished{false};
    std::atomic<bool> _lastMotion{false};
    std::atomic<uint32_t> _lastPublishedMs{0};
};

} // namespace CSI
} // namespace WIFISENSING
