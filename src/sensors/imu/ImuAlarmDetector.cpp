#include "ImuAlarmDetector.h"

#include "ImuMath.h"

#include <algorithm>
#include <cmath>

namespace IMU {

namespace {

float clearThresholdDeg(const ImuAlarmConfig& config) {
    return (std::max)(0.0f, config.tiltThresholdDeg - (std::max)(0.0f, config.tiltHysteresisDeg));
}

bool shockActive(const ImuAlarmConfig& config, const ImuMetrics& metrics) {
    return std::isfinite(metrics.accelDeltaG) &&
           config.accelDeltaThresholdG > 0.0f &&
           metrics.accelDeltaG >= config.accelDeltaThresholdG;
}

bool shockDecisionAvailable(const ImuMetrics& metrics) {
    return std::isfinite(metrics.accelDeltaG);
}

bool tiltDecisionAvailable(const ImuAlarmConfig& config,
                           const ImuMetrics& metrics) {
    return config.baselineValid && metrics.baselineValid &&
           std::isfinite(metrics.tiltDeg);
}

bool tiltTriggerActive(const ImuAlarmConfig& config, const ImuMetrics& metrics) {
    return config.baselineValid &&
           std::isfinite(metrics.tiltDeg) &&
           metrics.tiltDeg >= config.tiltThresholdDeg;
}

bool tiltStillActiveForClear(const ImuAlarmConfig& config, const ImuMetrics& metrics) {
    return config.baselineValid &&
           std::isfinite(metrics.tiltDeg) &&
           metrics.tiltDeg >= clearThresholdDeg(config);
}

}  // namespace

void ImuAlarmDetector::reset() {
    _triggered = false;
    _triggerHoldActive = false;
    _clearHoldActive = false;
    _triggerHoldStartMs = 0;
    _clearHoldStartMs = 0;
    _activeReason = ImuAlarmReason::None;
}

void ImuAlarmDetector::restoreRetainedTrigger(bool triggered) {
    if (!triggered) {
        return;
    }

    _triggered = true;
    _triggerHoldActive = false;
    _clearHoldActive = false;
    _triggerHoldStartMs = 0;
    _clearHoldStartMs = 0;
    // The retained alarm summary does not persist whether tilt or shock caused
    // the trigger. Treat it as tilt so clearing requires the more conservative
    // baseline-backed proof instead of one low acceleration sample.
    _activeReason = ImuAlarmReason::Tilt;
}

ImuAlarmStatus ImuAlarmDetector::update(const ImuAlarmConfig& config,
                                        const ImuMetrics& metrics,
                                        uint32_t nowMs) {
    ImuAlarmStatus status;
    status.enabled = config.enabled;
    status.sampleFresh = metrics.sampleFresh;
    status.baselineValid = config.baselineValid;
    status.currentTiltDeg = metrics.tiltDeg;
    status.accelDeltaG = metrics.accelDeltaG;

    if (!config.enabled) {
        reset();
        status.decisionValid = true;
        return status;
    }

    if (!metrics.sampleFresh) {
        // Missing data is unknown, not evidence that a confirmed tamper ended.
        // Preserve the last stable binary decision and break both temporal
        // proofs so time spent without fresh samples cannot satisfy a hold.
        _triggerHoldActive = false;
        _clearHoldActive = false;
        _triggerHoldStartMs = 0;
        _clearHoldStartMs = 0;
        status.triggered = _triggered;
        status.triggerValue = _triggered ? 1.0f : 0.0f;
        status.reason = metrics.sampleTimestampKnown ? ImuAlarmReason::Stale : ImuAlarmReason::Unavailable;
        return status;
    }

    status.decisionValid = true;

    const bool shock = shockActive(config, metrics);
    const bool tiltTrigger = tiltTriggerActive(config, metrics);

    if (_triggered) {
        const bool stillActive = shock || tiltStillActiveForClear(config, metrics);
        if (stillActive) {
            _clearHoldActive = false;
            _clearHoldStartMs = 0;
        } else {
            const bool clearEvidenceAvailable =
                _activeReason == ImuAlarmReason::Shock
                    ? shockDecisionAvailable(metrics)
                    : tiltDecisionAvailable(config, metrics);
            if (!clearEvidenceAvailable) {
                // Fresh transport alone is insufficient. A tilt alarm cannot be
                // cleared without a valid baseline/finite tilt, while a shock
                // alarm requires a finite acceleration decision. Break the
                // previous clear proof and publish no binary edge.
                _clearHoldActive = false;
                _clearHoldStartMs = 0;
                status.decisionValid = false;
                status.triggered = true;
                status.triggerValue = 1.0f;
                status.reason =
                    _activeReason != ImuAlarmReason::Shock &&
                            (!config.baselineValid || !metrics.baselineValid)
                        ? ImuAlarmReason::NoBaseline
                        : ImuAlarmReason::Unavailable;
                return status;
            }
            if (!_clearHoldActive) {
                _clearHoldActive = true;
                _clearHoldStartMs = nowMs;
            }
            status.pendingClear = true;
            status.clearHoldElapsedMs = MATH::elapsedMs(nowMs, _clearHoldStartMs);
            if (status.clearHoldElapsedMs >= config.tiltClearHoldMs) {
                reset();
                status.pendingClear = false;
                status.clearHoldElapsedMs = 0;
            }
        }
    }

    if (!_triggered) {
        if (shock) {
            _triggered = true;
            _activeReason = ImuAlarmReason::Shock;
            _triggerHoldActive = false;
            _triggerHoldStartMs = 0;
        } else if (tiltTrigger) {
            if (!_triggerHoldActive) {
                _triggerHoldActive = true;
                _triggerHoldStartMs = nowMs;
            }
            status.pendingTrigger = true;
            status.triggerHoldElapsedMs = MATH::elapsedMs(nowMs, _triggerHoldStartMs);
            if (status.triggerHoldElapsedMs >= config.tiltHoldMs) {
                _triggered = true;
                _activeReason = ImuAlarmReason::Tilt;
                status.pendingTrigger = false;
            }
        } else {
            _triggerHoldActive = false;
            _triggerHoldStartMs = 0;
        }
    }

    status.triggered = _triggered;
    status.triggerValue = _triggered ? 1.0f : 0.0f;

    if (_triggered) {
        status.reason = _activeReason;
    } else if (status.pendingTrigger) {
        status.reason = ImuAlarmReason::Tilt;
    } else if (!config.baselineValid) {
        status.reason = ImuAlarmReason::NoBaseline;
    } else {
        status.reason = ImuAlarmReason::None;
    }

    return status;
}

}  // namespace IMU
