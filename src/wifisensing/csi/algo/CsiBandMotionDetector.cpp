#include "CsiBandMotionDetector.h"

#include <cmath>
#include <cstring>
#include <cstdlib>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace WIFISENSING {
namespace CSI {
namespace {

constexpr float kGainMin = 0.25f;
constexpr float kGainMax = 4.0f;
constexpr float kGlobalHighRatioThreshold = 0.60f;
constexpr uint32_t kGlobalHighHoldMs = 500;
constexpr float kBaselineEwmaAlpha = 0.005f;

bool g_forceStorageAllocationFailure = false;

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

float clampFinite(float value, float minValue, float maxValue, float fallback) {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return clampValue(value, minValue, maxValue);
}

uint32_t elapsedMs(uint32_t nowMs, uint32_t sinceMs) {
    return nowMs - sinceMs;
}

CsiMotionStorage* allocateStorage() {
    if (g_forceStorageAllocationFailure) {
        return nullptr;
    }
#ifdef NATIVE_BUILD
    return static_cast<CsiMotionStorage*>(std::calloc(1, sizeof(CsiMotionStorage)));
#else
    return static_cast<CsiMotionStorage*>(
        heap_caps_calloc(1, sizeof(CsiMotionStorage), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif
}

void freeStorage(CsiMotionStorage* storage) {
#ifdef NATIVE_BUILD
    std::free(storage);
#else
    heap_caps_free(storage);
#endif
}

} // namespace

const char* toString(CsiMotionState state) {
    switch (state) {
        case CsiMotionState::Disabled:
            return "disabled";
        case CsiMotionState::NeedsConfiguration:
            return "needs_configuration";
        case CsiMotionState::Calibrating:
            return "calibrating";
        case CsiMotionState::Monitoring:
            return "monitoring";
        case CsiMotionState::MotionCandidate:
            return "motion_candidate";
        case CsiMotionState::MotionConfirmed:
            return "motion_confirmed";
        case CsiMotionState::NoisyEnvironment:
            return "noisy_environment";
        case CsiMotionState::Unavailable:
            return "unavailable";
        default:
            return "unknown";
    }
}

const char* toString(CsiMotionResetReason reason) {
    switch (reason) {
        case CsiMotionResetReason::None:
            return "none";
        case CsiMotionResetReason::Startup:
            return "startup";
        case CsiMotionResetReason::ConfigChange:
            return "config_change";
        case CsiMotionResetReason::ManualCalibration:
            return "manual_calibration";
        case CsiMotionResetReason::WidthChange:
            return "width_change";
        case CsiMotionResetReason::UnavailableStorage:
            return "unavailable_storage";
        case CsiMotionResetReason::InvalidFrame:
            return "invalid_frame";
        default:
            return "unknown";
    }
}

CsiBandMotionDetector::~CsiBandMotionDetector() {
    end();
}

bool CsiBandMotionDetector::begin() {
    if (_storage) {
        return true;
    }

    _storage = allocateStorage();
    _storageAllocationFailed = (_storage == nullptr);
    if (!_storage) {
        _lastResetReason = CsiMotionResetReason::UnavailableStorage;
        _snapshot = makeSnapshot(CsiMotionState::Unavailable);
        return false;
    }

    resetBaseline(CsiMotionResetReason::Startup);
    _snapshot = makeSnapshot(_config.enabled ? CsiMotionState::NeedsConfiguration : CsiMotionState::Disabled);
    return true;
}

void CsiBandMotionDetector::end() {
    if (_storage) {
        freeStorage(_storage);
        _storage = nullptr;
    }
    _storageAllocationFailed = false;
    _snapshot = makeSnapshot(CsiMotionState::Unavailable);
}

void CsiBandMotionDetector::configure(const CsiMotionConfig& config) {
    _config = config;
    normalizeConfig(_config);

    if (!_storage) {
        _lastResetReason = CsiMotionResetReason::UnavailableStorage;
        _snapshot = makeSnapshot(CsiMotionState::Unavailable);
    } else if (!_config.enabled) {
        resetBaseline(CsiMotionResetReason::ConfigChange);
        _snapshot = makeSnapshot(CsiMotionState::Disabled);
    } else if (_config.bandCount == 0) {
        resetBaseline(CsiMotionResetReason::ConfigChange);
        _snapshot = makeSnapshot(CsiMotionState::NeedsConfiguration);
    } else {
        resetBaseline(CsiMotionResetReason::ConfigChange);
        _snapshot = makeSnapshot(CsiMotionState::Calibrating);
    }
}

void CsiBandMotionDetector::resetBaseline(CsiMotionResetReason reason) {
    _lastResetReason = reason;
    _baselineReady = false;
    _motion = false;
    _baselineFramesSeen = 0;
    _candidateSinceMs = 0;
    _clearSinceMs = 0;
    _noisyClearSinceMs = 0;
    _globalHighSinceMs = 0;
    clearBaselineArrays();
    _snapshot.baselineReady = false;
    _snapshot.motion = false;
    _snapshot.noisy = false;
    _snapshot.needsCalibration = _config.enabled && _config.bandCount > 0;
    _snapshot.lastResetReason = _lastResetReason;
}

CsiMotionSnapshot CsiBandMotionDetector::process(const CsiPacket& packet, uint32_t nowMs) {
    if (!_storage || _storageAllocationFailed) {
        _motion = false;
        _snapshot = makeSnapshot(CsiMotionState::Unavailable);
        return _snapshot;
    }

    if (!_config.enabled) {
        _motion = false;
        _snapshot = makeSnapshot(CsiMotionState::Disabled);
        return _snapshot;
    }

    const uint16_t width = static_cast<uint16_t>(packet.len / 2);
    if (width < 8 || width > MAX_CSI_SUBCARRIERS) {
        _motion = false;
        _lastResetReason = CsiMotionResetReason::InvalidFrame;
        _snapshot = makeSnapshot(CsiMotionState::Unavailable);
        return _snapshot;
    }

    _framesSeen++;

    if (width != _width) {
        resetBaselineForWidth(width);
    }

    if (_config.bandCount == 0) {
        _motion = false;
        _snapshot = makeSnapshot(CsiMotionState::NeedsConfiguration);
        return _snapshot;
    }

    computeEnergy(packet, width);

    if (!_baselineReady) {
        accumulateBaseline(width);
        if (_baselineFramesSeen >= _config.baselineFrames) {
            finalizeBaseline(width);
            _snapshot = makeSnapshot(CsiMotionState::Monitoring);
        } else {
            _snapshot = makeSnapshot(CsiMotionState::Calibrating);
        }
        return _snapshot;
    }

    const ScoreResult score = scoreSelectedBands(width);
    const bool noisy = updateNoisyGate(score, nowMs);
    CsiMotionState state = _snapshot.state;

    const bool broadDisturbance = score.globalHighRatio >= kGlobalHighRatioThreshold;

    if (noisy) {
        _motion = false;
        _noisyClearSinceMs = 0;
        state = CsiMotionState::NoisyEnvironment;
    } else if (_snapshot.state == CsiMotionState::NoisyEnvironment) {
        _motion = false;
        if (score.score < _config.clearThreshold) {
            if (_noisyClearSinceMs == 0) {
                _noisyClearSinceMs = nowMs;
            }
            state = (elapsedMs(nowMs, _noisyClearSinceMs) >= _config.clearHoldMs)
                        ? CsiMotionState::Monitoring
                        : CsiMotionState::NoisyEnvironment;
        } else {
            _noisyClearSinceMs = 0;
            state = CsiMotionState::NoisyEnvironment;
        }
    } else if (broadDisturbance) {
        _motion = false;
        _candidateSinceMs = 0;
        _clearSinceMs = 0;
        state = CsiMotionState::Monitoring;
    } else {
        switch (_snapshot.state) {
            case CsiMotionState::Monitoring:
            case CsiMotionState::Calibrating:
            case CsiMotionState::NeedsConfiguration:
            case CsiMotionState::Disabled:
            case CsiMotionState::Unavailable:
                if (score.score >= _config.enterThreshold) {
                    _candidateSinceMs = nowMs;
                    _clearSinceMs = 0;
                    state = CsiMotionState::MotionCandidate;
                } else {
                    _motion = false;
                    _candidateSinceMs = 0;
                    state = CsiMotionState::Monitoring;
                }
                break;

            case CsiMotionState::MotionCandidate:
                if (score.score < _config.clearThreshold) {
                    _motion = false;
                    _candidateSinceMs = 0;
                    state = CsiMotionState::Monitoring;
                } else if (elapsedMs(nowMs, _candidateSinceMs) >= _config.holdMs) {
                    _motion = true;
                    _clearSinceMs = 0;
                    state = CsiMotionState::MotionConfirmed;
                } else {
                    _motion = false;
                    state = CsiMotionState::MotionCandidate;
                }
                break;

            case CsiMotionState::MotionConfirmed:
                if (score.score < _config.clearThreshold) {
                    if (_clearSinceMs == 0) {
                        _clearSinceMs = nowMs;
                    }
                    if (elapsedMs(nowMs, _clearSinceMs) >= _config.clearHoldMs) {
                        _motion = false;
                        _clearSinceMs = 0;
                        _candidateSinceMs = 0;
                        state = CsiMotionState::Monitoring;
                    } else {
                        _motion = true;
                        state = CsiMotionState::MotionConfirmed;
                    }
                } else {
                    _motion = true;
                    _clearSinceMs = 0;
                    state = CsiMotionState::MotionConfirmed;
                }
                break;

            case CsiMotionState::NoisyEnvironment:
                state = CsiMotionState::Monitoring;
                break;
        }
    }

    if (_config.autoRecalibration &&
        !_motion &&
        state != CsiMotionState::NoisyEnvironment &&
        score.score < _config.clearThreshold) {
        updateBaselineEwma(width);
    }

    _snapshot = makeSnapshot(state, &score);
    return _snapshot;
}

void CsiBandMotionDetector::normalizeConfig(CsiMotionConfig& config) const {
    config.bandCount = clampValue<uint8_t>(config.bandCount, 0, MAX_CSI_ALARM_BANDS);
    config.baselineFrames = clampValue<uint16_t>(config.baselineFrames, 30, 1000);
    config.topK = clampValue<uint8_t>(config.topK, 1, 32);
    config.sensitivity = normalizeCsiMotionSensitivity(config.sensitivity);
    const CsiMotionSensitivityPreset sensitivityPreset = csiMotionSensitivityPreset(config.sensitivity);
    config.enterThreshold = sensitivityPreset.enterThreshold;
    config.clearThreshold = sensitivityPreset.clearThreshold;
    config.holdMs = clampValue<uint16_t>(config.holdMs, 100, 10000);
    config.clearHoldMs = clampValue<uint16_t>(config.clearHoldMs, 100, 30000);
    config.minNoise = clampFinite(config.minNoise, 0.1f, 1000.0f, 4.0f);
    config.minEnergy = clampFinite(config.minEnergy, 0.0f, 10000.0f, 4.0f);
    config.noisyScoreThreshold = clampFinite(
        config.noisyScoreThreshold,
        config.enterThreshold,
        500.0f,
        80.0f);

    for (uint8_t i = 0; i < config.bandCount; ++i) {
        CsiBandRange& band = config.bands[i];
        band.start = clampValue<uint16_t>(band.start, 0, MAX_CSI_SUBCARRIERS - 1);
        band.end = clampValue<uint16_t>(band.end, 0, MAX_CSI_SUBCARRIERS - 1);
        if (band.start > band.end) {
            const uint16_t tmp = band.start;
            band.start = band.end;
            band.end = tmp;
        }
    }

    for (uint8_t i = config.bandCount; i < MAX_CSI_ALARM_BANDS; ++i) {
        config.bands[i] = {};
    }
}

void CsiBandMotionDetector::resetBaselineForWidth(uint16_t width) {
    _width = width;
    resetBaseline(CsiMotionResetReason::WidthChange);
    _snapshot.width = width;
}

void CsiBandMotionDetector::clearBaselineArrays() {
    if (!_storage) {
        return;
    }
    std::memset(_storage->energy, 0, sizeof(_storage->energy));
    std::memset(_storage->mean, 0, sizeof(_storage->mean));
    std::memset(_storage->m2, 0, sizeof(_storage->m2));
    std::memset(_storage->noise, 0, sizeof(_storage->noise));
    std::memset(_storage->valid, 0, sizeof(_storage->valid));
    std::memset(_storage->topScores, 0, sizeof(_storage->topScores));
}

void CsiBandMotionDetector::computeEnergy(const CsiPacket& packet, uint16_t width) {
    float gain = packet.compensate_gain;
    if (!std::isfinite(gain)) {
        gain = 1.0f;
    }
    gain = clampValue(gain, kGainMin, kGainMax);
    const float gainSquared = gain * gain;

    for (uint16_t i = 0; i < width; ++i) {
        const float re = static_cast<float>(packet.buf[2 * i]);
        const float im = static_cast<float>(packet.buf[(2 * i) + 1]);
        _storage->energy[i] = (re * re + im * im) * gainSquared;
    }
}

void CsiBandMotionDetector::accumulateBaseline(uint16_t width) {
    _baselineFramesSeen++;
    const float n = static_cast<float>(_baselineFramesSeen);
    for (uint16_t i = 0; i < width; ++i) {
        const float value = _storage->energy[i];
        const float delta = value - _storage->mean[i];
        _storage->mean[i] += delta / n;
        const float delta2 = value - _storage->mean[i];
        _storage->m2[i] += delta * delta2;
    }
}

void CsiBandMotionDetector::finalizeBaseline(uint16_t width) {
    _baselineReady = true;
    for (uint16_t i = 0; i < width; ++i) {
        const float variance = (_baselineFramesSeen > 1)
                                   ? (_storage->m2[i] / static_cast<float>(_baselineFramesSeen - 1))
                                   : 0.0f;
        const float noise = std::sqrt(variance);
        _storage->noise[i] = std::isfinite(noise) ? clampValue(noise, _config.minNoise, 1000000.0f)
                                                  : _config.minNoise;
        _storage->valid[i] =
            (std::isfinite(_storage->mean[i]) && _storage->mean[i] >= _config.minEnergy) ? 1u : 0u;
    }
}

CsiBandMotionDetector::ScoreResult CsiBandMotionDetector::scoreSelectedBands(uint16_t width) {
    ScoreResult result;
    const uint8_t topK = _config.topK;
    for (uint8_t i = 0; i < topK; ++i) {
        _storage->topScores[i] = 0.0f;
    }

    for (uint8_t bandIndex = 0; bandIndex < _config.bandCount; ++bandIndex) {
        const CsiBandRange& band = _config.bands[bandIndex];
        if (band.start >= width) {
            continue;
        }

        const uint16_t end = band.end >= width ? static_cast<uint16_t>(width - 1) : band.end;
        for (uint16_t i = band.start; i <= end; ++i) {
            result.selectedCarrierCount++;
            if (_storage->valid[i] == 0) {
                continue;
            }

            const float energy = _storage->energy[i];
            const float mean = _storage->mean[i];
            const float noise = _storage->noise[i] > _config.minNoise ? _storage->noise[i] : _config.minNoise;
            if (!std::isfinite(energy) || !std::isfinite(mean) || !std::isfinite(noise)) {
                continue;
            }

            const float z = std::fabs(energy - mean) / noise;
            result.validSelectedCarrierCount++;
            for (uint8_t pos = 0; pos < topK; ++pos) {
                if (z <= _storage->topScores[pos]) {
                    continue;
                }
                for (uint8_t move = topK - 1; move > pos; --move) {
                    _storage->topScores[move] = _storage->topScores[move - 1];
                }
                _storage->topScores[pos] = z;
                break;
            }
        }
    }

    const uint8_t topCount =
        static_cast<uint8_t>(result.validSelectedCarrierCount < topK ? result.validSelectedCarrierCount : topK);
    if (topCount > 0) {
        float sum = 0.0f;
        for (uint8_t i = 0; i < topCount; ++i) {
            sum += _storage->topScores[i];
        }
        result.score = sum / static_cast<float>(topCount);
    }

    uint16_t highCarrierCount = 0;
    for (uint16_t i = 0; i < width; ++i) {
        if (_storage->valid[i] == 0) {
            continue;
        }
        result.validCarrierCount++;
        const float noise = _storage->noise[i] > _config.minNoise ? _storage->noise[i] : _config.minNoise;
        const float z = std::fabs(_storage->energy[i] - _storage->mean[i]) / noise;
        if (std::isfinite(z) && z >= _config.enterThreshold) {
            highCarrierCount++;
        }
    }

    if (result.validCarrierCount > 0) {
        result.globalHighRatio =
            static_cast<float>(highCarrierCount) / static_cast<float>(result.validCarrierCount);
    }

    return result;
}

bool CsiBandMotionDetector::updateNoisyGate(const ScoreResult& score, uint32_t nowMs) {
    const uint16_t minValidSelected = static_cast<uint16_t>((_config.topK / 2) > 4 ? (_config.topK / 2) : 4);
    if (score.validSelectedCarrierCount < minValidSelected) {
        _globalHighSinceMs = 0;
        return true;
    }

    if (score.score >= _config.noisyScoreThreshold) {
        _globalHighSinceMs = 0;
        return true;
    }

    if (score.globalHighRatio >= kGlobalHighRatioThreshold) {
        if (_globalHighSinceMs == 0) {
            _globalHighSinceMs = nowMs;
        }
        if (elapsedMs(nowMs, _globalHighSinceMs) >= kGlobalHighHoldMs) {
            return true;
        }
    } else {
        _globalHighSinceMs = 0;
    }

    return false;
}

void CsiBandMotionDetector::updateBaselineEwma(uint16_t width) {
    for (uint16_t i = 0; i < width; ++i) {
        if (_storage->valid[i] == 0) {
            continue;
        }
        const float energy = _storage->energy[i];
        if (!std::isfinite(energy)) {
            continue;
        }
        const float delta = energy - _storage->mean[i];
        _storage->mean[i] += kBaselineEwmaAlpha * delta;
        const float absDelta = std::fabs(delta);
        _storage->noise[i] =
            clampValue(((1.0f - kBaselineEwmaAlpha) * _storage->noise[i]) + (kBaselineEwmaAlpha * absDelta),
                       _config.minNoise,
                       1000000.0f);
    }
}

CsiMotionSnapshot CsiBandMotionDetector::makeSnapshot(CsiMotionState state, const ScoreResult* score) {
    CsiMotionSnapshot snapshot;
    snapshot.state = state;
    snapshot.motion = _motion && state == CsiMotionState::MotionConfirmed;
    snapshot.baselineReady = _baselineReady;
    snapshot.noisy = state == CsiMotionState::NoisyEnvironment;
    snapshot.needsCalibration =
        _config.enabled &&
        _config.bandCount > 0 &&
        !_baselineReady &&
        state != CsiMotionState::Unavailable;
    snapshot.score = score ? score->score : 0.0f;
    snapshot.confidence =
        (_config.enterThreshold > 0.0f)
            ? clampValue(snapshot.score / _config.enterThreshold, 0.0f, 1.0f)
            : 0.0f;
    snapshot.framesSeen = _framesSeen;
    snapshot.width = _width;
    snapshot.bandCount = _config.bandCount;
    snapshot.lastResetReason = _lastResetReason;
    if (score) {
        snapshot.selectedCarrierCount = score->selectedCarrierCount;
        snapshot.validCarrierCount = score->validCarrierCount;
        fillVisualizationBins(snapshot, _width);
    }
    return snapshot;
}

void CsiBandMotionDetector::fillVisualizationBins(CsiMotionSnapshot& snapshot, uint16_t width) const {
    if (!_storage || !_baselineReady || width == 0) {
        snapshot.visualizationBinCount = 0;
        return;
    }

    snapshot.visualizationBinCount = 64;
    const float scale = _config.enterThreshold > 0.0f ? _config.enterThreshold : 6.0f;

    for (uint8_t bin = 0; bin < 64; ++bin) {
        const uint16_t start = static_cast<uint16_t>((static_cast<uint32_t>(bin) * width) / 64u);
        uint16_t end = static_cast<uint16_t>(((static_cast<uint32_t>(bin + 1u) * width) / 64u));
        if (end <= start) {
            end = static_cast<uint16_t>(start + 1u);
        }
        if (end > width) {
            end = width;
        }

        float sum = 0.0f;
        uint16_t count = 0;
        for (uint16_t i = start; i < end; ++i) {
            if (_storage->valid[i] == 0) {
                continue;
            }
            const float energy = _storage->energy[i];
            const float mean = _storage->mean[i];
            const float noise = _storage->noise[i] > _config.minNoise ? _storage->noise[i] : _config.minNoise;
            if (!std::isfinite(energy) || !std::isfinite(mean) || !std::isfinite(noise)) {
                continue;
            }
            sum += std::fabs(energy - mean) / noise;
            count++;
        }

        const float avg = count > 0 ? sum / static_cast<float>(count) : 0.0f;
        const float normalized = clampValue(avg / scale, 0.0f, 1.0f);
        snapshot.visualizationBins[bin] = static_cast<uint8_t>(normalized * 255.0f);
    }
}

#ifdef UNIT_TEST
namespace TEST_HOOKS {
void setCsiMotionStorageAllocationFailure(bool fail) {
    g_forceStorageAllocationFailure = fail;
}
} // namespace TEST_HOOKS
#endif

} // namespace CSI
} // namespace WIFISENSING
