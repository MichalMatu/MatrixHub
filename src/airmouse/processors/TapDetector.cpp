#include "../../system/logging/Logging.h"
#include "TapDetector.h"
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "AirMouseTap"

namespace AIRMOUSE {

namespace {
float deltaFromG2(float totalG2) {
    const float baseline = CONFIG::AIR_MOUSE::PROCESSOR::GRAVITY_BASELINE_G;
    return fabsf(sqrtf(totalG2) - baseline);
}
} // namespace

TapDetector::TapDetector() {
    updateThresholds();
    const float baseline = CONFIG::AIR_MOUSE::PROCESSOR::GRAVITY_BASELINE_G;
    _lastTotalG2 = baseline * baseline;
}

void TapDetector::updateConfig(const AirMouseConfig& config) {
    _tapThresholdG = config.tapThresholdG;
    _clickDebounceMs = config.clickDebounceMs;
    _doubleClickWindowMs = config.doubleClickWindowMs;
    updateThresholds();
}

void TapDetector::reset() {
    _lastClickTime = 0;
    _lastTapTime = 0;
    _tapCount = 0;
    const float baseline = CONFIG::AIR_MOUSE::PROCESSOR::GRAVITY_BASELINE_G;
    _lastTotalG2 = baseline * baseline;
}

TapGesture TapDetector::process(float ax, float ay, float az, uint32_t now) {
    TapGesture emitted = TapGesture::NONE;

    // Dispatch accumulated taps when the window expires (matches ButtonHandler semantics).
    if (_tapCount > 0 && (now - _lastTapTime > _doubleClickWindowMs)) {
        emitted = static_cast<TapGesture>(_tapCount);
        LOGD("Tap gesture: %u tap(s)", _tapCount);
        _tapCount = 0;
        _lastTapTime = 0;
    }

    // Gravity compensation: compare squared magnitude against squared thresholds.
    const float totalG2 = ax*ax + ay*ay + az*az;
    _lastTotalG2 = totalG2;

    // Debounce window prevents repeated triggers on one impact.
    if ((totalG2 > _tapThresholdHigh2 || totalG2 < _tapThresholdLow2) &&
        (now - _lastClickTime > _clickDebounceMs)) {
        const float deltaG = deltaFromG2(totalG2);
        handleTap(now, deltaG);
    }

    return emitted;
}

void TapDetector::handleTap(uint32_t now, float deltaG) {
    _lastClickTime = now;

    if (_tapCount == 0) {
        _tapCount = 1;
        _lastTapTime = now;
        LOGD("Tap detected (deltaG=%.2f), count=1", deltaG);
        return;
    }

    // If within the same window, increment. Otherwise start a new sequence.
    if (now - _lastTapTime <= _doubleClickWindowMs) {
        _tapCount = (_tapCount < 3) ? (_tapCount + 1) : 3;
        _lastTapTime = now;
        LOGD("Tap detected (deltaG=%.2f), count=%u", deltaG, _tapCount);
        return;
    }

    // Window expired — start a new sequence with this tap.
    _tapCount = 1;
    _lastTapTime = now;
    LOGD("Tap detected (deltaG=%.2f), count=1 (new sequence)", deltaG);
}

float TapDetector::getLastDeltaG() const {
    return deltaFromG2(_lastTotalG2);
}

void TapDetector::updateThresholds() {
    const float baseline = CONFIG::AIR_MOUSE::PROCESSOR::GRAVITY_BASELINE_G;
    float low = baseline - _tapThresholdG;
    if (low < 0.0f) low = 0.0f;
    const float high = baseline + _tapThresholdG;
    _tapThresholdLow2 = low * low;
    _tapThresholdHigh2 = high * high;
}

} // namespace AIRMOUSE
