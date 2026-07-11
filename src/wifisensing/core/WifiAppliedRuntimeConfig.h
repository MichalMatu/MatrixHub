#pragma once

#include <cstdint>

namespace WIFISENSING {

/**
 * Last effective RSSI worker state and the configuration applied to it.
 *
 * The owner must serialize access. WifiSensingService uses its lifecycle mutex,
 * while native tests can exercise the state machine directly.
 */
struct WifiAppliedRuntimeConfigSnapshot {
    bool enabled = false;
    uint32_t sampleIntervalMs = 0;
    float varianceThreshold = 0.0f;
    uint32_t generation = 0;
    bool known = true;
};

class WifiAppliedRuntimeConfig {
public:
    const WifiAppliedRuntimeConfigSnapshot& snapshot() const {
        return _snapshot;
    }

    void markEnabled(uint32_t sampleIntervalMs, float varianceThreshold) {
        if (_snapshot.enabled &&
            _snapshot.sampleIntervalMs == sampleIntervalMs &&
            _snapshot.varianceThreshold == varianceThreshold) {
            return;
        }

        _snapshot.enabled = true;
        _snapshot.sampleIntervalMs = sampleIntervalMs;
        _snapshot.varianceThreshold = varianceThreshold;
        _snapshot.generation++;
    }

    void markDisabled() {
        if (!_snapshot.enabled) {
            return;
        }

        _snapshot.enabled = false;
        _snapshot.generation++;
    }

    bool needsReconcile(bool desiredEnabled,
                        uint32_t desiredSampleIntervalMs,
                        float desiredVarianceThreshold,
                        bool runtimeRunning,
                        bool runtimeResourcesPresent) const {
        if (!desiredEnabled) {
            return runtimeRunning || runtimeResourcesPresent || _snapshot.enabled;
        }

        return !runtimeRunning ||
               !_snapshot.enabled ||
               _snapshot.sampleIntervalMs != desiredSampleIntervalMs ||
               _snapshot.varianceThreshold != desiredVarianceThreshold;
    }

private:
    WifiAppliedRuntimeConfigSnapshot _snapshot{};
};

}  // namespace WIFISENSING
