#pragma once

#include <cstdint>

namespace WIFISENSING {

/**
 * Wrap-safe bounded exponential backoff for RSSI/CSI runtime reconciliation.
 */
class WifiRuntimeReconcileSchedule {
public:
    explicit WifiRuntimeReconcileSchedule(uint32_t baseDelayMs = 5000,
                                          uint32_t maxDelayMs = 60000)
        : _baseDelayMs(baseDelayMs),
          _maxDelayMs(maxDelayMs >= baseDelayMs ? maxDelayMs : baseDelayMs),
          _backoffMs(baseDelayMs) {}

    bool isDue(uint32_t nowMs) const {
        return !_scheduled || static_cast<int32_t>(nowMs - _nextAttemptMs) >= 0;
    }

    void markHealthy(uint32_t nowMs) {
        _backoffMs = _baseDelayMs;
        _nextAttemptMs = nowMs + _baseDelayMs;
        _scheduled = true;
    }

    void markFailure(uint32_t nowMs) {
        _nextAttemptMs = nowMs + _backoffMs;
        _scheduled = true;
        _backoffMs = _backoffMs >= (_maxDelayMs / 2)
                         ? _maxDelayMs
                         : _backoffMs * 2;
        if (_backoffMs > _maxDelayMs) {
            _backoffMs = _maxDelayMs;
        }
    }

    uint32_t nextAttemptMs() const {
        return _nextAttemptMs;
    }

    uint32_t backoffMs() const {
        return _backoffMs;
    }

private:
    uint32_t _baseDelayMs;
    uint32_t _maxDelayMs;
    uint32_t _nextAttemptMs = 0;
    uint32_t _backoffMs;
    bool _scheduled = false;
};

}  // namespace WIFISENSING
