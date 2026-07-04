#pragma once

#include <Arduino.h>
#include <cmath>

namespace UTILS {
namespace MATH {

/**
 * @brief 1-Euro Filter for adaptive smoothing
 * 
 * Filters noisy signals (like mouse cursor movement).
 * Reduces jitter when stationary (low cutoff), responsive when moving (high cutoff).
 * 
 * Based on http://cristal.univ-lille.fr/~casiez/1euro/
 */
class OneEuroFilter {
public:
    OneEuroFilter(float minCutoff, float beta, float dCutoff);
    float filter(float value, uint32_t timestampMs);
    void reset();

    // Setters for runtime tuning
    void setMinCutoff(float minCutoff) { _minCutoff = minCutoff; }
    void setBeta(float beta) { _beta = beta; }
    void setDCutoff(float dCutoff) { _dCutoff = dCutoff; }

private:
    float alpha(float cutoff, float dt);
    float lowPass(float x, float prev, float a);

    float _minCutoff, _beta, _dCutoff;
    float _x, _dx;
    uint32_t _lastTime;
    bool _initialized;
};

} // namespace MATH
} // namespace UTILS
