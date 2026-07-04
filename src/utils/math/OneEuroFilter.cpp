#include "OneEuroFilter.h"

namespace UTILS {
namespace MATH {

OneEuroFilter::OneEuroFilter(float minCutoff, float beta, float dCutoff)
    : _minCutoff(minCutoff), _beta(beta), _dCutoff(dCutoff),
      _x(0), _dx(0), _lastTime(0), _initialized(false) {}

float OneEuroFilter::filter(float value, uint32_t timestampMs) {
    if (!_initialized) {
        _x = value;
        _dx = 0;
        _lastTime = timestampMs;
        _initialized = true;
        return value;
    }

    float dt = (timestampMs - _lastTime) / 1000.0f;
    // Handle millis() overflow (after ~49 days) or duplicates
    if (timestampMs < _lastTime || dt <= 0) {
        dt = 0.001f;
    }
    _lastTime = timestampMs;

    // Derivative estimation (low-pass filtered)
    float edx = (value - _x) / dt;
    _dx = lowPass(edx, _dx, alpha(_dCutoff, dt));

    // Adaptive cutoff: fast movement = higher cutoff = less filtering
    float cutoff = _minCutoff + _beta * fabsf(_dx);

    // Filter the value
    _x = lowPass(value, _x, alpha(cutoff, dt));
    return _x;
}

void OneEuroFilter::reset() {
    _initialized = false;
    _x = 0;
    _dx = 0;
    _lastTime = 0;
}

float OneEuroFilter::alpha(float cutoff, float dt) {
    float tau = 1.0f / (2.0f * PI * cutoff);
    return 1.0f / (1.0f + tau / dt);
}

float OneEuroFilter::lowPass(float x, float prev, float a) {
    return a * x + (1.0f - a) * prev;
}

} // namespace MATH
} // namespace UTILS
