// [ARCHIVED][DISABLED] Legacy CSI motion algorithm (frozen reference implementation).
// Kept for future reactivation experiments on ESP32.
// Uncomment #if 0 / #endif to re-enable.
#if 0
#define LOG_TAG "CsiAlgo"

#include "CsiAlgorithm.h"
#include "../../../../system/logging/Logging.h"
#include "filters/CsiFilters.h"

namespace WIFISENSING {
namespace CSI {

CsiAlgorithm::CsiAlgorithm() {
    // MVS History: Single stream of float (Turbulence values)
    _turbulenceHistory.fill(0.0f);
    
    // Default: Select ALL subcarriers initially
    _selectedIndicesCount = SUB_CARRIERS;
    for(int i=0; i<SUB_CARRIERS; i++) {
        _selectedIndices[i] = i;
    }
    
    // Pre-allocate persistent amplitude buffer
    _currentAmplitudes.fill(0.0f);
    
    // Initialize MVS Filters (Single instance)
    _mvsHampel.init(7, 3.0f); // Window 7, Threshold 3 sigma
    _mvsLowPass.init(0.15f);  // Alpha 0.15

    reset();
}

CsiAlgorithm::~CsiAlgorithm() {
    if (_calibrationBuffer) {
        free(_calibrationBuffer);
        _calibrationBuffer = nullptr;
    }
}

void CsiAlgorithm::reset() {
    _turbulenceHistory.fill(0.0f);
    _historyIndex = 0;
    _samplesCount = 0;
    _currentScore = 0.0f;
    
    // Reset State Machine
    _state = State::WARMUP;
    _warmupFrames = 0;
    _calibrationFrames = 0;
    
    if (_calibrationBuffer) {
        free(_calibrationBuffer);
        _calibrationBuffer = nullptr;
    }
    
    // Reset filters
    _mvsHampel.reset();
    _mvsLowPass.reset();
}

void CsiAlgorithm::calculateAmplitudes(const CsiPacket& packet, std::array<float, SUB_CARRIERS>& outAmps) {

    if (packet.len == 0) return;

    // Packet.buf contains pairs of int8_t (Real, Imag)
    // len is in bytes. num_subcarriers = len / 2
    int numSc = packet.len / 2;
    if (numSc > SUB_CARRIERS) numSc = SUB_CARRIERS; // Clamp

    for (int i = 0; i < numSc; i++) {
        int8_t real = packet.buf[i * 2];
        int8_t imag = packet.buf[i * 2 + 1];
        // Optimize: Calculating Energy instead of true Amplitude to avoid expensive FPU sqrtf
        outAmps[i] = (float)(real * real + imag * imag) * packet.compensate_gain;
    }
    for (int i = numSc; i < SUB_CARRIERS; ++i) {
        outAmps[i] = 0.0f;
    }
}

float CsiAlgorithm::calculateVariance(const float* buffer, int count) {
    if (count < 2) return 0.0f;
    
    float sum = 0.0f;
    for(int i=0; i<count; i++) sum += buffer[i];
    
    float mean = sum / count;
    float sumSq = 0.0f;
    
    for(int i=0; i<count; i++) {
        float diff = buffer[i] - mean;
        sumSq += diff * diff;
    }
    
    return sumSq / count;
}

float CsiAlgorithm::calculateSpatialTurbulence(const std::array<float, SUB_CARRIERS>& amps) {
    // Spatial Turbulence = StdDev of amplitudes of SELECTED subcarriers
    if (_selectedIndicesCount == 0) return 0.0f;

    float sum = 0.0f;
    int count = 0;

    // 1. Mean
    for (int i = 0; i < _selectedIndicesCount; ++i) {
        int idx = _selectedIndices[i];
        if (idx >= 0 && idx < SUB_CARRIERS) {
            sum += amps[idx];
            count++;
        }
    }
    if (count < 2) return 0.0f;
    float mean = sum / count;

    // 2. Variance
    float sumSq = 0.0f;
    for (int i = 0; i < _selectedIndicesCount; ++i) {
        int idx = _selectedIndices[i];
        if (idx >= 0 && idx < SUB_CARRIERS) {
            float diff = amps[idx] - mean;
            sumSq += diff * diff;
        }
    }
    
    // Return Variance (without expensive sqrtf calculation, MVS autocalibration handles scaling)
    return sumSq / count;
}

bool CsiAlgorithm::process(const CsiPacket& packet) {
    // 1. Extract current amplitudes (Reuse persistent buffer)
    calculateAmplitudes(packet, _currentAmplitudes);
    
    int numSc = packet.len / 2;
    if (numSc > SUB_CARRIERS) numSc = SUB_CARRIERS;

    // 4. State Machine Processing
    if (_state == State::WARMUP) {
        _warmupFrames++;
        if (_warmupFrames >= WARMUP_COUNT) {
            LOGI("CSI: Warmup done. Starting Calibration...");
            _state = State::CALIBRATING;
            _calibrationFrames = 0;
            // Allocate calibration buffer in PSRAM
            size_t bufSize = CALIBRATION_COUNT * SUB_CARRIERS * sizeof(float);
            _calibrationBuffer = (float*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
            if (!_calibrationBuffer) {
                LOGE("CSI: Failed to allocate calibration buffer! Skipping to RUNNING.");
                _state = State::RUNNING;
            }
        }
        return false; // No motion in warmup
    }

    if (_state == State::CALIBRATING) {
        processCalibration(_currentAmplitudes);
        return false;
    }

    // --- RUNNING STATE (MVS Pipeline) ---

    // 5. Calculate Spatial Turbulence (One value per frame)
    float rawTurbulence = calculateSpatialTurbulence(_currentAmplitudes);

    // 6. Filter Turbulence Stream
    float filteredTurbulence = _mvsHampel.apply(rawTurbulence);
    filteredTurbulence = _mvsLowPass.apply(filteredTurbulence);

    // 7. Update History (Circular Buffer)
    _turbulenceHistory[_historyIndex] = filteredTurbulence;
    _historyIndex = (_historyIndex + 1) % MVS_WINDOW;
    if (_samplesCount < MVS_WINDOW) _samplesCount++;

    // 8. Need minimum samples to detect?
    if (_samplesCount < 10) return false;

    // 9. Calculate Motion Score = Variance of Turbulence History (MVS)
    // We calculate variance over the sliding window of turbulence values
    float score = calculateVariance(_turbulenceHistory.data(), _samplesCount);
    
    // Scale up for readability (Variance is usually small)
    _currentScore = score * 100.0f; 

    // LOGD(LOG_TAG, "MVS Score: %.4f (Thr: %.2f)", _currentScore, _threshold);

    return _currentScore > _threshold;
}

    // ... (helper functions) ...

void CsiAlgorithm::processCalibration(const std::array<float, SUB_CARRIERS>& amplitudes) {
    if (!_calibrationBuffer) return;
    
    if (_calibrationFrames < CALIBRATION_COUNT) {
        // Copy current frame to buffer
        // Buffer layout: Frame 0 [0..63], Frame 1 [0..63]...
        memcpy(
            &_calibrationBuffer[_calibrationFrames * SUB_CARRIERS], 
            amplitudes.data(), 
            SUB_CARRIERS * sizeof(float)
        );
        _calibrationFrames++;
    }

    if (_calibrationFrames >= CALIBRATION_COUNT) {
        finalizeCalibration();
        _state = State::RUNNING;
        
        // Free buffer to save memory
        free(_calibrationBuffer);
        _calibrationBuffer = nullptr;
    }
}

void CsiAlgorithm::finalizeCalibration() {
    LOGI("CSI: Finalizing Calibration...");
    
    std::array<SubcarrierStat, SUB_CARRIERS> stats;

    for (int sc = 0; sc < SUB_CARRIERS; ++sc) {
        // Compute Variance for this SC across all calibration frames
        float sum = 0.0f;
        float sumSq = 0.0f;
        int count = CALIBRATION_COUNT;

        for (int f = 0; f < count; ++f) {
            float val = _calibrationBuffer[f * SUB_CARRIERS + sc];
            sum += val;
            sumSq += val * val;
        }

        float mean = sum / count;
        float variance = (sumSq / count) - (mean * mean);
        
        stats[sc] = {sc, variance, mean};
    }

    // Sort by Variance (Lower is better)
    // Ignore SCs with very low mean (dead subcarriers)
    std::sort(stats.begin(), stats.end(), [](const SubcarrierStat& a, const SubcarrierStat& b) {
        // Penalize dead subcarriers (mean < 3.0) by pushing them to end
        bool a_dead = a.mean < 3.0f;
        bool b_dead = b.mean < 3.0f;
        if (a_dead != b_dead) return b_dead; // If one is dead, the other is better
        
        return a.variance < b.variance;
    });

    _selectedIndicesCount = 0;
    LOGI("CSI: Selected Subcarriers (Top %d):", SELECTED_SC_COUNT);
    
    for (int i = 0; i < SELECTED_SC_COUNT && i < SUB_CARRIERS; ++i) {
        _selectedIndices[_selectedIndicesCount++] = stats[i].index;
        // Compact log?
        // LOGI("  SC #%d (Var: %.2f, Mean: %.1f)", stats[i].index, stats[i].variance, stats[i].mean);
    }
    
    // Fix: Ensure we have at least SOME indices if all are bad
    if (_selectedIndicesCount == 0) {
        for(int i=0; i<SUB_CARRIERS; i++) _selectedIndices[_selectedIndicesCount++] = i;
    }
    
    std::sort(_selectedIndices.begin(), _selectedIndices.begin() + _selectedIndicesCount);
    
    // --- Autocalibrate MVS Threshold ---
    // Reprocess the calibration buffer to simulate what MVS would have seen
    // and establish the baseline variance (Noise Floor of Turbulence).

    // 1. Extract Turbulence trajectory from calibration buffer
    std::array<float, CALIBRATION_COUNT> calTurbulence;

    for (int f = 0; f < CALIBRATION_COUNT; ++f) {
        // Construct a temporary vector for this frame's amplitudes
        std::array<float, SUB_CARRIERS> frameAmps;
        for(int sc=0; sc<SUB_CARRIERS; sc++) {
            frameAmps[sc] = _calibrationBuffer[f * SUB_CARRIERS + sc];
        }
        
        // Calculate spatial turbulence for this frame
        float turb = calculateSpatialTurbulence(frameAmps);
        calTurbulence[f] = turb;
    }
    
    // 2. Calculate Variance of this Turbulence stream (Baseline MVS Score)
    float baselineVariance = calculateVariance(calTurbulence.data(), CALIBRATION_COUNT);
    float baselineScore = baselineVariance * 100.0f; // Scale to match process()

    // 3. Set Threshold
    // Logic: 5.0x baseline variance + offset
    float newThreshold = (baselineScore * 5.0f) + 0.5f;
    
    // Safety Clamps
    if (newThreshold < 1.0f) newThreshold = 1.0f; 
    if (newThreshold > 50.0f) newThreshold = 50.0f;

    _threshold = newThreshold;
    LOGI("CSI: MVS Baseline Score: %.4f, Auto-Threshold set to: %.2f", baselineScore, _threshold);
    
    LOGI("CSI: Calibration Complete. MVS Active.");
}

} // namespace CSI
} // namespace WIFISENSING
#endif // disabled motion detection algorithm
