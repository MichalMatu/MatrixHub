#pragma once

#include <stdint.h>

namespace ALARMS {

class CsiAlarmEdgeLatch {
public:
    enum class PendingDecision : uint8_t {
        None,
        Rising,
        Clear,
    };

    void submit(bool motion) {
        if (!_latestKnown || motion != _latestMotion) {
            if (motion) {
                _risingPending = true;
            } else {
                _clearPending = true;
            }
        }
        _latestMotion = motion;
        _latestKnown = true;
    }

    PendingDecision next() const {
        // Never let a clear coalesce away a rising edge. After the rising pass,
        // complete() converges to the latest state on the following pass.
        if (_risingPending) {
            return PendingDecision::Rising;
        }
        if (_clearPending) {
            return PendingDecision::Clear;
        }
        return PendingDecision::None;
    }

    void complete(PendingDecision decision) {
        if (decision == PendingDecision::Rising) {
            _risingPending = false;
            _clearPending = _latestKnown && !_latestMotion;
        } else if (decision == PendingDecision::Clear) {
            _clearPending = false;
            _risingPending = _latestKnown && _latestMotion;
        }
    }

    bool hasPending() const {
        return _risingPending || _clearPending;
    }

private:
    bool _latestKnown = false;
    bool _latestMotion = false;
    bool _risingPending = false;
    bool _clearPending = false;
};

// The state machine is source-agnostic. Keep the historical CSI name for API
// compatibility while using the semantic alias for other boolean alarm inputs.
using BooleanAlarmEdgeLatch = CsiAlarmEdgeLatch;

} // namespace ALARMS
