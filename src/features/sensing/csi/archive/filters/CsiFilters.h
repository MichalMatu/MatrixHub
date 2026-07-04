#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace WIFISENSING {
namespace CSI {
namespace UTILS {

// =============================================================================
// Low-pass Filter (1st order Butterworth IIR)
// =============================================================================
class LowPassFilter {
public:
    LowPassFilter() = default;
    
    // Constructor matching CsiAlgorithm usage
    explicit LowPassFilter(float cutoff_hz) {
        init(cutoff_hz);
    }

    void init(float cutoff_hz, float sample_rate_hz = 100.0f) {
        if (cutoff_hz < 0.1f) cutoff_hz = 0.1f;
        
        _cutoff_hz = cutoff_hz;
        _initialized = false;
        _x_prev = 0.0f;
        _y_prev = 0.0f;

        // Calculate coefficients (Bilinear Transform)
        float wc = tanf(M_PI * cutoff_hz / sample_rate_hz);
        float k = 1.0f + wc;
        _b0 = wc / k;
        _a1 = (wc - 1.0f) / k;
    }

    float apply(float value) {
        if (!_initialized) {
            _x_prev = value;
            _y_prev = value;
            _initialized = true;
            return value;
        }

        // y[n] = b0*(x[n] + x[n-1]) - a1*y[n-1]
        // But the espectre impl used: y = b0 * x + b0 * x_prev - a1 * y_prev
        // Note: Generic 1st order IIR: y[n] = (1-alpha)*y[n-1] + alpha*x[n] -- this is simpler but let's stick to espectre's math
        
        float y = _b0 * value + _b0 * _x_prev - _a1 * _y_prev;
        
        _x_prev = value;
        _y_prev = y;

        return y;
    }

    void reset() {
        _x_prev = 0.0f;
        _y_prev = 0.0f;
        _initialized = false;
    }

private:
    float _b0 = 0.0f;
    float _a1 = 0.0f;
    float _x_prev = 0.0f;
    float _y_prev = 0.0f;
    float _cutoff_hz = 10.0f;
    bool _initialized = false;
};

// =============================================================================
// Hampel Filter (MAD-based outlier removal)
// =============================================================================
class HampelFilter {
public:
    static constexpr float MAD_SCALE_FACTOR = 1.4826f;
    static constexpr uint8_t WINDOW_MAX = 11;

    HampelFilter() = default;

    HampelFilter(uint8_t window_size, float threshold) {
        init(window_size, threshold);
    }

    void init(uint8_t window_size = 7, float threshold = 4.0f) {
        if (window_size < 3) window_size = 3;
        if (window_size > WINDOW_MAX) window_size = WINDOW_MAX;

        _window_size = window_size;
        _threshold = threshold;
        _index = 0;
        _count = 0;
        memset(_buffer, 0, sizeof(_buffer));
    }

    float apply(float value) {
        // Add to circular buffer
        _buffer[_index] = value;
        _index = (_index + 1) % _window_size;
        if (_count < _window_size) _count++;

        // Need at least 3 samples
        if (_count < 3) return value;

        // Copy active part to temp buffer for processing
        int n = _count;
        float sorted[WINDOW_MAX];
        float deviations[WINDOW_MAX];

        // 1. Calculate Median
        // Actually, for a sliding window filter we usually take the window *centered* on the current sample?
        // Espectre implementation seems to filter the *current* incoming sample based on history? 
        // "hampel_filter_turbulence" adds to buffer then computes median of buffer.
        // Yes, it filters the *latest* sample based on the window distribution. 
        // Note: Strictly Hampel identifies outliers in the window. If the *latest* (which we just added) is outlier, we replace it.
        
        memcpy(sorted, _buffer, n * sizeof(float));
        // Simple insertion sort is fast for small N
        for (int i = 1; i < n; i++) {
            float key = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > key) {
                sorted[j + 1] = sorted[j];
                j--;
            }
            sorted[j + 1] = key;
        }

        float median = (n % 2 == 1) 
                     ? sorted[n / 2] 
                     : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0f;

        // 2. Calculate Median Absolute Deviation (MAD)
        for (int i = 0; i < n; i++) {
            deviations[i] = std::abs(_buffer[i] - median);
        }
        
        // Sort deviations
        for (int i = 1; i < n; i++) {
            float key = deviations[i];
            int j = i - 1;
            while (j >= 0 && deviations[j] > key) {
                deviations[j + 1] = deviations[j];
                j--;
            }
            deviations[j + 1] = key;
        }

        float mad = (n % 2 == 1) 
                  ? deviations[n / 2] 
                  : (deviations[n / 2 - 1] + deviations[n / 2]) / 2.0f;

        // 3. Check for outlier
        // value is the input 'value' which is also somewhere in _buffer (actually it was just added).
        // Wait, if we added it to buffer, it's there.
        // The check is: |value - median| > threshold * 1.4826 * MAD
        
        float deviation = std::abs(value - median);
        
        if (deviation > _threshold * MAD_SCALE_FACTOR * mad) {
            // Replace with median
            // NOTE: Should we update the buffer? Espectre doesn't seem to update the history buffer with the corrected value in 'hampel_filter_turbulence'.
            // "return median;"
            // If we don't update internal buffer, the outlier remains in history influencing next steps.
            // Standard Hampel replaces the outlier in the window.
            // Espectre implementation: 
            // state->buffer[state->index] = turbulence; (Adds raw value)
            // ... returns median.
            // So Espectre does NOT correct the history. This is 'outlier suppression' for output only.
            // We will stick to Espectre logic to be safe.
            return median;
        }

        return value;
    }

    void reset() {
        _index = 0;
        _count = 0;
        memset(_buffer, 0, sizeof(_buffer));
    }

private:
    float _buffer[WINDOW_MAX];
    uint8_t _window_size = 7;
    uint8_t _index = 0;
    uint8_t _count = 0;
    float _threshold = 4.0f;
};

} // namespace UTILS
} // namespace CSI
} // namespace WIFISENSING
