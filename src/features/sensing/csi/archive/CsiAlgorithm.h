#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include "esp_wifi_types.h" // For wifi_csi_info_t structs if needed, or we use our CsiPacket
#include "../../../../wifisensing/csi/data/CsiTypes.h"
#include "filters/CsiFilters.h"


namespace WIFISENSING {
namespace CSI {

// Forward decl or include packet definition
// Packet definition from CsiTypes.h 

class CsiAlgorithm {
public:
    CsiAlgorithm();
    ~CsiAlgorithm();

    /**
     * @brief Process a new CSI packet and return true if motion is detected.
     * @param packet The CSI packet to process.
     * @return true if motion score > threshold.
     */
    bool process(const CsiPacket& packet);

    float getMotionScore() const { return _currentScore; }
    void setThreshold(float threshold) { _threshold = threshold; }
    
    // Reset history/state
    void reset();

private:
    static const int SUB_CARRIERS = 64; // HT20
    static const int MVS_WINDOW = 40;   // Window for Moving Variance (~3s)

    // Circular buffer for Spatial Turbulence History (Single stream)
    // Replaces the large [20][64] amplitude history matrix
    std::array<float, MVS_WINDOW> _turbulenceHistory;

    int _historyIndex = 0;
    int _samplesCount = 0;

    float _currentScore = 0.0f;
    float _threshold = 4.0f; 

    void calculateAmplitudes(const CsiPacket& packet, std::array<float, SUB_CARRIERS>& outAmps);
    float calculateSpatialTurbulence(const std::array<float, SUB_CARRIERS>& amps);
    float calculateVariance(const float* buffer, int count);
    
    // MVS Filters (Single instance applied to the Turbulence stream)
    WIFISENSING::CSI::UTILS::HampelFilter _mvsHampel;
    WIFISENSING::CSI::UTILS::LowPassFilter _mvsLowPass;

    // --- Subcarrier Selection (Calibration) ---
    enum class State {
        WARMUP,      // Wait for filters/AGC to settle
        CALIBRATING, // Collect stats for noise floor
        RUNNING      // Normal operation on selected subcarriers
    };

    State _state = State::WARMUP;
    int _warmupFrames = 0;
    int _calibrationFrames = 0;

    static const int WARMUP_COUNT = 20;      // Frames to ignore at start
    static const int CALIBRATION_COUNT = 100; // Frames to analyze
    static const int SELECTED_SC_COUNT = 32;  // Top 50% best SCs

    // Indices of the "cleanest" subcarriers
    int _selectedIndicesCount = 0;
    std::array<int, SUB_CARRIERS> _selectedIndices;

    struct SubcarrierStat {
        int index;
        float variance;
        float mean;
    };

    // Temporary buffer for calibration phase [Frame][Subcarrier]
    float* _calibrationBuffer = nullptr; 

    // Persistent buffer for current frame amplitudes (Avoids re-allocation)
    std::array<float, SUB_CARRIERS> _currentAmplitudes; 

    void processCalibration(const std::array<float, SUB_CARRIERS>& amplitudes);
    void finalizeCalibration();

}; // CsiAlgorithm

} // namespace CSI
} // namespace WIFISENSING
