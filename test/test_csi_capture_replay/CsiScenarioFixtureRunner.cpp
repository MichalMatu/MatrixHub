#include "CsiScenarioFixtureRunner.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include "CsiCaptureFixtureCodec.h"
#include "CsiCaptureReplay.h"

namespace WIFISENSING {
namespace CSI {
namespace NATIVE_CAPTURE {
namespace {

constexpr uint32_t kUint32HalfRange = 0x80000000u;
constexpr size_t kMaxScenarioBytes = 256u * 1024u;
constexpr size_t kMaxTimelineIntervals = 1024;
constexpr const char* kScenarioSchema = "matrixhub.csi.scenario/v1";
constexpr const char* kCaptureFileName = "frames.mhcf";
constexpr const char* kScenarioFileName = "scenario.json";

bool fail(std::string& error, const std::string& message) {
    error = message;
    return false;
}

bool readString(JsonObjectConst object,
                const char* key,
                std::string& output,
                std::string& error) {
    JsonVariantConst value = object[key];
    if (!value.is<const char*>()) {
        return fail(error, std::string("missing or invalid string: ") + key);
    }
    output = value.as<const char*>();
    return true;
}

bool readBool(JsonObjectConst object,
              const char* key,
              bool& output,
              std::string& error) {
    JsonVariantConst value = object[key];
    if (!value.is<bool>()) {
        return fail(error, std::string("missing or invalid boolean: ") + key);
    }
    output = value.as<bool>();
    return true;
}

bool readU32(JsonObjectConst object,
             const char* key,
             uint32_t& output,
             std::string& error) {
    JsonVariantConst value = object[key];
    if (!value.is<uint32_t>()) {
        return fail(error, std::string("missing or invalid uint32: ") + key);
    }
    output = value.as<uint32_t>();
    if (output >= kUint32HalfRange) {
        return fail(error, std::string("value exceeds modular half-range: ") + key);
    }
    return true;
}

bool readFloat(JsonObjectConst object,
               const char* key,
               float& output,
               std::string& error) {
    JsonVariantConst value = object[key];
    if (!value.is<float>()) {
        return fail(error, std::string("missing or invalid float: ") + key);
    }
    output = value.as<float>();
    if (!std::isfinite(output)) {
        return fail(error, std::string("non-finite float: ") + key);
    }
    return true;
}

bool isOneOf(const std::string& value,
             std::initializer_list<const char*> allowed) {
    for (const char* candidate : allowed) {
        if (value == candidate) {
            return true;
        }
    }
    return false;
}

bool parseDetectorConfig(JsonObjectConst object,
                         ScenarioSourcePolicy sourcePolicy,
                         CsiMotionConfig& config,
                         std::string& error) {
    bool enabled = false;
    bool autoRecalibration = false;
    uint32_t baselineFrames = 0;
    uint32_t topK = 0;
    uint32_t holdMs = 0;
    uint32_t clearHoldMs = 0;
    uint32_t sensitivity = 0;
    float enterThreshold = 0.0f;
    float clearThreshold = 0.0f;
    float minNoise = 0.0f;
    float minEnergy = 0.0f;
    float noisyThreshold = 0.0f;
    if (!readBool(object, "enabled", enabled, error) ||
        !readU32(object, "baseline_frames", baselineFrames, error) ||
        !readU32(object, "top_k", topK, error) ||
        !readFloat(object, "enter_threshold", enterThreshold, error) ||
        !readFloat(object, "clear_threshold", clearThreshold, error) ||
        !readU32(object, "hold_ms", holdMs, error) ||
        !readU32(object, "clear_hold_ms", clearHoldMs, error) ||
        !readFloat(object, "min_noise", minNoise, error) ||
        !readFloat(object, "min_energy", minEnergy, error) ||
        !readFloat(object, "noisy_threshold", noisyThreshold, error) ||
        !readBool(object, "auto_recalibration", autoRecalibration, error) ||
        !readU32(object, "sensitivity", sensitivity, error)) {
        return false;
    }
    if (sourcePolicy == ScenarioSourcePolicy::RealDeviceOnly && !enabled) {
        return fail(error, "real-device detector_config.enabled must be true");
    }
    if (baselineFrames < 30 || baselineFrames > 1000 ||
        topK < 1 || topK > 32 ||
        holdMs < 100 || holdMs > 10000 ||
        clearHoldMs < 100 || clearHoldMs > 30000 ||
        sensitivity > 2 ||
        minNoise < 0.1f || minNoise > 1000.0f ||
        minEnergy < 0.0f || minEnergy > 10000.0f ||
        noisyThreshold < enterThreshold || noisyThreshold > 500.0f) {
        return fail(error, "detector_config value is outside production bounds");
    }
    const auto preset = csiMotionSensitivityPreset(static_cast<uint8_t>(sensitivity));
    if (std::fabs(enterThreshold - preset.enterThreshold) > 0.01f ||
        std::fabs(clearThreshold - preset.clearThreshold) > 0.01f) {
        return fail(error, "detector thresholds disagree with sensitivity preset");
    }

    JsonVariantConst bandsValue = object["bands"];
    if (!bandsValue.is<JsonArrayConst>()) {
        return fail(error, "detector_config.bands must be an array");
    }
    JsonArrayConst bands = bandsValue.as<JsonArrayConst>();
    if (bands.size() == 0 || bands.size() > MAX_CSI_ALARM_BANDS) {
        return fail(error, "detector_config.bands must contain 1..4 ranges");
    }

    config = {};
    config.enabled = enabled;
    config.bandCount = static_cast<uint8_t>(bands.size());
    size_t bandIndex = 0;
    for (JsonVariantConst bandValue : bands) {
        if (!bandValue.is<JsonObjectConst>()) {
            return fail(error, "detector band must be an object");
        }
        JsonObjectConst band = bandValue.as<JsonObjectConst>();
        uint32_t start = 0;
        uint32_t end = 0;
        if (!readU32(band, "start", start, error) ||
            !readU32(band, "end", end, error)) {
            return false;
        }
        if (start > end || end >= MAX_CSI_SUBCARRIERS) {
            return fail(error, "detector band is outside the canonical CSI width");
        }
        config.bands[bandIndex++] = {
            static_cast<uint16_t>(start),
            static_cast<uint16_t>(end),
        };
    }
    config.baselineFrames = static_cast<uint16_t>(baselineFrames);
    config.topK = static_cast<uint8_t>(topK);
    config.enterThreshold = enterThreshold;
    config.clearThreshold = clearThreshold;
    config.holdMs = static_cast<uint16_t>(holdMs);
    config.clearHoldMs = static_cast<uint16_t>(clearHoldMs);
    config.minNoise = minNoise;
    config.minEnergy = minEnergy;
    config.noisyScoreThreshold = noisyThreshold;
    config.autoRecalibration = autoRecalibration;
    config.sensitivity = static_cast<uint8_t>(sensitivity);
    return true;
}

bool parseMotion(const std::string& value,
                 GroundTruthMotion& motion,
                 std::string& error) {
    if (value == "none") {
        motion = GroundTruthMotion::None;
        return true;
    }
    if (value == "present") {
        motion = GroundTruthMotion::Present;
        return true;
    }
    if (value == "unknown") {
        motion = GroundTruthMotion::Unknown;
        return true;
    }
    return fail(error, "ground-truth motion has an unsupported value");
}

bool parseAcceptance(JsonObjectConst object,
                     ReplayAcceptance& acceptance,
                     std::string& error) {
    bool reviewed = false;
    if (!readBool(object, "reviewed", reviewed, error) || !reviewed) {
        return fail(error, "acceptance thresholds are not reviewed");
    }
    return readU32(object, "max_false_positive_ms", acceptance.maxFalsePositiveMs, error) &&
           readU32(object, "max_false_negative_ms", acceptance.maxFalseNegativeMs, error) &&
           readU32(object, "max_invalid_decision_ms", acceptance.maxInvalidDecisionMs, error) &&
           readU32(object, "max_detection_latency_ms", acceptance.maxDetectionLatencyMs, error) &&
           readU32(object, "max_clear_latency_ms", acceptance.maxClearLatencyMs, error) &&
           readU32(object, "max_motion_dropout_ms", acceptance.maxMotionDropoutMs, error) &&
           readU32(object, "max_missed_motion_intervals", acceptance.maxMissedMotionIntervals, error) &&
           readU32(object, "max_uncleared_transitions", acceptance.maxUnclearedTransitions, error);
}

size_t intervalAt(const std::vector<GroundTruthInterval>& timeline,
                  uint32_t timeMs) {
    auto it = std::upper_bound(
        timeline.begin(),
        timeline.end(),
        timeMs,
        [](uint32_t value, const GroundTruthInterval& interval) {
            return value < interval.endMs;
        });
    return static_cast<size_t>(std::distance(timeline.begin(), it));
}

bool decisionValidAt(const ReplayObservation& observation, uint32_t timeMs) {
    return observation.decisionValid &&
           timeMs >= observation.lastEvidenceMs &&
           timeMs - observation.lastEvidenceMs <= CSI_MOTION_STALE_AFTER_MS;
}

bool predictionAt(const std::vector<ReplayObservation>& observations,
                  uint32_t timeMs,
                  bool& motion) {
    auto it = std::upper_bound(
        observations.begin(),
        observations.end(),
        timeMs,
        [](uint32_t value, const ReplayObservation& observation) {
            return value < observation.relativeMs;
        });
    if (it == observations.begin()) {
        return false;
    }
    --it;
    if (!decisionValidAt(*it, timeMs)) {
        return false;
    }
    motion = it->motion;
    return true;
}

bool firstPrediction(const std::vector<ReplayObservation>& observations,
                     uint32_t startMs,
                     uint32_t endMs,
                     bool wanted,
                     uint32_t& foundAtMs) {
    bool atStart = false;
    if (predictionAt(observations, startMs, atStart) && atStart == wanted) {
        foundAtMs = startMs;
        return true;
    }
    for (const auto& observation : observations) {
        if (observation.relativeMs <= startMs) {
            continue;
        }
        if (observation.relativeMs >= endMs) {
            break;
        }
        if (decisionValidAt(observation, observation.relativeMs) &&
            observation.motion == wanted) {
            foundAtMs = observation.relativeMs;
            return true;
        }
    }
    return false;
}

struct PredictionRun {
    uint32_t count = 0;
    uint32_t longestMs = 0;
};

PredictionRun measurePredictionRuns(const std::vector<ReplayObservation>& observations,
                                    uint32_t captureDurationMs,
                                    uint32_t startMs,
                                    uint32_t endMs,
                                    bool wanted) {
    PredictionRun result;
    uint32_t currentMs = 0;
    bool inRun = false;
    for (size_t index = 0; index < observations.size(); ++index) {
        const uint32_t segmentStart = observations[index].relativeMs;
        const uint32_t segmentEnd = index + 1 < observations.size()
                                        ? observations[index + 1].relativeMs
                                        : captureDurationMs;
        const uint32_t overlapStart = std::max(segmentStart, startMs);
        const uint32_t overlapEnd = std::min(segmentEnd, endMs);
        if (overlapStart >= overlapEnd) {
            continue;
        }
        if (observations[index].motion == wanted) {
            if (!inRun) {
                inRun = true;
                result.count++;
            }
            currentMs += overlapEnd - overlapStart;
            result.longestMs = std::max(result.longestMs, currentMs);
        } else {
            inRun = false;
            currentMs = 0;
        }
    }
    return result;
}

struct MotionWindow {
    uint32_t startMs = 0;
    uint32_t endMs = 0;
};

std::vector<MotionWindow> motionWindows(const CsiScenario& scenario) {
    std::vector<MotionWindow> windows;
    for (const auto& interval : scenario.timeline) {
        if (interval.motion != GroundTruthMotion::Present) {
            continue;
        }
        if (!windows.empty() && windows.back().endMs == interval.startMs) {
            windows.back().endMs = interval.endMs;
        } else {
            windows.push_back({interval.startMs, interval.endMs});
        }
    }
    return windows;
}

bool followingKnownNoneWindow(const CsiScenario& scenario,
                              const MotionWindow& motion,
                              MotionWindow& noneWindow) {
    for (size_t index = 0; index < scenario.timeline.size(); ++index) {
        const auto& interval = scenario.timeline[index];
        if (interval.endMs <= motion.endMs) {
            continue;
        }
        if (interval.motion == GroundTruthMotion::Unknown) {
            continue;
        }
        if (interval.motion == GroundTruthMotion::Present) {
            return false;
        }
        noneWindow = {interval.startMs, interval.endMs};
        while (index + 1 < scenario.timeline.size() &&
               scenario.timeline[index + 1].motion == GroundTruthMotion::None &&
               noneWindow.endMs == scenario.timeline[index + 1].startMs) {
            noneWindow.endMs = scenario.timeline[++index].endMs;
        }
        return true;
    }
    return false;
}

void appendViolation(std::vector<std::string>& violations,
                     const char* metric,
                     uint64_t actual,
                     uint64_t allowed) {
    std::ostringstream stream;
    stream << metric << "=" << actual << " exceeds " << allowed;
    violations.push_back(stream.str());
}

bool readTextFile(const std::filesystem::path& path,
                  size_t maxBytes,
                  std::string& output,
                  std::string& error) {
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return fail(error, "cannot stat " + path.string() + ": " + ec.message());
    }
    if (size == 0 || size > maxBytes) {
        return fail(error, "file size is outside the allowed range: " + path.string());
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fail(error, "cannot open " + path.string());
    }
    output.resize(static_cast<size_t>(size));
    input.read(output.data(), static_cast<std::streamsize>(output.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(output.size())) {
        return fail(error, "short read from " + path.string());
    }
    return true;
}

bool readBinaryFile(const std::filesystem::path& path,
                    std::vector<uint8_t>& output,
                    std::string& error) {
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return fail(error, "cannot stat " + path.string() + ": " + ec.message());
    }
    if (size == 0 || size > CSI_CAPTURE_MAX_BYTES) {
        return fail(error, "capture size is outside the MHCF limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fail(error, "cannot open " + path.string());
    }
    output.resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(output.data()),
               static_cast<std::streamsize>(output.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(output.size())) {
        return fail(error, "short read from " + path.string());
    }
    return true;
}

// Compact test-only SHA-256. Fixture integrity is checked again by the Python
// preflight, but the native runner remains fail-closed when invoked directly.
class Sha256 {
public:
    Sha256() { reset(); }

    void update(const uint8_t* data, size_t size) {
        if (!data && size != 0) {
            return;
        }
        _totalBytes += size;
        while (size > 0) {
            const size_t take = std::min(size, _buffer.size() - _bufferSize);
            std::memcpy(_buffer.data() + _bufferSize, data, take);
            _bufferSize += take;
            data += take;
            size -= take;
            if (_bufferSize == _buffer.size()) {
                transform(_buffer.data());
                _bufferSize = 0;
            }
        }
    }

    std::array<uint8_t, 32> finish() {
        const uint64_t totalBits = _totalBytes * 8u;
        _buffer[_bufferSize++] = 0x80;
        if (_bufferSize > 56) {
            std::fill(_buffer.begin() + _bufferSize, _buffer.end(), 0);
            transform(_buffer.data());
            _bufferSize = 0;
        }
        std::fill(_buffer.begin() + _bufferSize, _buffer.begin() + 56, 0);
        for (size_t index = 0; index < 8; ++index) {
            _buffer[63 - index] = static_cast<uint8_t>(totalBits >> (index * 8u));
        }
        transform(_buffer.data());

        std::array<uint8_t, 32> digest{};
        for (size_t index = 0; index < _state.size(); ++index) {
            digest[index * 4] = static_cast<uint8_t>(_state[index] >> 24u);
            digest[index * 4 + 1] = static_cast<uint8_t>(_state[index] >> 16u);
            digest[index * 4 + 2] = static_cast<uint8_t>(_state[index] >> 8u);
            digest[index * 4 + 3] = static_cast<uint8_t>(_state[index]);
        }
        return digest;
    }

private:
    static uint32_t rotateRight(uint32_t value, uint32_t count) {
        return (value >> count) | (value << (32u - count));
    }

    void reset() {
        _state = {
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
        };
        _buffer.fill(0);
        _bufferSize = 0;
        _totalBytes = 0;
    }

    void transform(const uint8_t* block) {
        static constexpr uint32_t constants[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
        };
        uint32_t words[64]{};
        for (size_t index = 0; index < 16; ++index) {
            words[index] = (static_cast<uint32_t>(block[index * 4]) << 24u) |
                           (static_cast<uint32_t>(block[index * 4 + 1]) << 16u) |
                           (static_cast<uint32_t>(block[index * 4 + 2]) << 8u) |
                           static_cast<uint32_t>(block[index * 4 + 3]);
        }
        for (size_t index = 16; index < 64; ++index) {
            const uint32_t s0 = rotateRight(words[index - 15], 7) ^
                                rotateRight(words[index - 15], 18) ^
                                (words[index - 15] >> 3u);
            const uint32_t s1 = rotateRight(words[index - 2], 17) ^
                                rotateRight(words[index - 2], 19) ^
                                (words[index - 2] >> 10u);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }

        uint32_t a = _state[0];
        uint32_t b = _state[1];
        uint32_t c = _state[2];
        uint32_t d = _state[3];
        uint32_t e = _state[4];
        uint32_t f = _state[5];
        uint32_t g = _state[6];
        uint32_t h = _state[7];
        for (size_t index = 0; index < 64; ++index) {
            const uint32_t sigma1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            const uint32_t choose = (e & f) ^ (~e & g);
            const uint32_t temp1 = h + sigma1 + choose + constants[index] + words[index];
            const uint32_t sigma0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = sigma0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        _state[0] += a;
        _state[1] += b;
        _state[2] += c;
        _state[3] += d;
        _state[4] += e;
        _state[5] += f;
        _state[6] += g;
        _state[7] += h;
    }

    std::array<uint32_t, 8> _state{};
    std::array<uint8_t, 64> _buffer{};
    size_t _bufferSize = 0;
    uint64_t _totalBytes = 0;
};

} // namespace

bool parseScenarioJson(const std::string& json,
                       const std::string& expectedFixtureId,
                       ScenarioSourcePolicy sourcePolicy,
                       CsiScenario& scenario,
                       std::string& error) {
    scenario = {};
    error.clear();
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(
        document,
        json,
        DeserializationOption::NestingLimit(12));
    if (jsonError) {
        return fail(error, std::string("invalid scenario JSON: ") + jsonError.c_str());
    }
    if (!document.is<JsonObject>()) {
        return fail(error, "scenario root must be an object");
    }
    JsonObjectConst root = document.as<JsonObjectConst>();
    std::string schema;
    if (!readString(root, "schema", schema, error) || schema != kScenarioSchema) {
        return fail(error, "unsupported scenario schema");
    }
    if (!readString(root, "fixture_id", scenario.fixtureId, error) ||
        (!expectedFixtureId.empty() && scenario.fixtureId != expectedFixtureId)) {
        return fail(error, "scenario fixture_id does not match its directory");
    }
    if (!readString(root, "capture_file", scenario.captureFile, error) ||
        scenario.captureFile != kCaptureFileName) {
        return fail(error, "scenario capture_file must be frames.mhcf");
    }
    if (!readString(root, "capture_sha256", scenario.captureSha256, error) ||
        scenario.captureSha256.size() != 71 ||
        scenario.captureSha256.rfind("sha256:", 0) != 0 ||
        !std::all_of(
            scenario.captureSha256.begin() + 7,
            scenario.captureSha256.end(),
            [](char value) {
                return (value >= '0' && value <= '9') ||
                       (value >= 'a' && value <= 'f');
            })) {
        return fail(error, "scenario capture_sha256 must be lowercase sha256:<64-hex>");
    }

    JsonVariantConst sourceValue = root["source"];
    if (!sourceValue.is<JsonObjectConst>()) {
        return fail(error, "scenario source must be an object");
    }
    JsonObjectConst source = sourceValue.as<JsonObjectConst>();
    if (!readString(source, "kind", scenario.sourceKind, error)) {
        return false;
    }
    if (sourcePolicy == ScenarioSourcePolicy::RealDeviceOnly) {
        if (scenario.sourceKind != "real_device") {
            return fail(error, "release fixture source.kind must be real_device");
        }
        for (const char* field : {
                 "board_env",
                 "firmware_version",
                 "firmware_commit",
                 "build_target",
                 "esp_platform",
                 "sdk_version",
                 "arduino_version",
             }) {
            std::string value;
            if (!readString(source, field, value, error) || value.empty() || value == "unknown") {
                return fail(error, std::string("incomplete real-device provenance: ") + field);
            }
        }
        std::string commit;
        bool firmwareDirty = true;
        bool firmwareIdentityVerified = false;
        if (!readString(source, "firmware_commit", commit, error) ||
            commit.size() != 40 ||
            !std::all_of(commit.begin(), commit.end(), [](char value) {
                return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
            })) {
            return fail(error, "real-device firmware_commit must be clean 40-hex");
        }
        if (!readBool(source, "firmware_dirty", firmwareDirty, error) || firmwareDirty ||
            !readBool(
                source,
                "firmware_identity_verified",
                firmwareIdentityVerified,
                error) ||
            !firmwareIdentityVerified) {
            return fail(error, "real-device firmware identity is not verified clean");
        }
    } else if (scenario.sourceKind != "real_device" &&
               scenario.sourceKind != "synthetic_unit") {
        return fail(error, "unit scenario source.kind must be real_device or synthetic_unit");
    }

    JsonVariantConst configValue = root["detector_config"];
    if (!configValue.is<JsonObjectConst>() ||
        !parseDetectorConfig(
            configValue.as<JsonObjectConst>(),
            sourcePolicy,
            scenario.detectorConfig,
            error)) {
        return false;
    }

    JsonVariantConst groundTruthValue = root["ground_truth"];
    if (!groundTruthValue.is<JsonObjectConst>()) {
        return fail(error, "ground_truth must be an object");
    }
    JsonObjectConst groundTruth = groundTruthValue.as<JsonObjectConst>();
    bool reviewed = false;
    if (!readBool(groundTruth, "reviewed", reviewed, error) || !reviewed) {
        return fail(error, "ground truth is not reviewed");
    }
    JsonVariantConst timelineValue = groundTruth["timeline"];
    if (!timelineValue.is<JsonArrayConst>()) {
        return fail(error, "ground_truth.timeline must be an array");
    }
    JsonArrayConst timeline = timelineValue.as<JsonArrayConst>();
    if (timeline.size() == 0 || timeline.size() > kMaxTimelineIntervals) {
        return fail(error, "ground_truth.timeline size is outside 1..1024");
    }
    uint32_t expectedStart = 0;
    uint32_t knownIntervals = 0;
    for (JsonVariantConst intervalValue : timeline) {
        if (!intervalValue.is<JsonObjectConst>()) {
            return fail(error, "ground-truth interval must be an object");
        }
        JsonObjectConst intervalObject = intervalValue.as<JsonObjectConst>();
        GroundTruthInterval interval;
        std::string motion;
        std::string occupancy;
        std::string environment;
        std::string evidence;
        std::string confidence;
        if (!readU32(intervalObject, "start_ms", interval.startMs, error) ||
            !readU32(intervalObject, "end_ms", interval.endMs, error) ||
            !readString(intervalObject, "motion", motion, error) ||
            !readString(intervalObject, "occupancy", occupancy, error) ||
            !readString(intervalObject, "environment", environment, error) ||
            !readString(intervalObject, "evidence", evidence, error) ||
            !readString(intervalObject, "confidence", confidence, error) ||
            !parseMotion(motion, interval.motion, error)) {
            return false;
        }
        if (interval.startMs != expectedStart || interval.startMs >= interval.endMs) {
            return fail(error, "ground-truth timeline must be ordered, gap-free, and non-empty");
        }
        if (!isOneOf(occupancy, {"empty", "occupied", "unknown"}) ||
            !isOneOf(environment, {"stable", "rf_disturbance", "reconnect", "unknown"}) ||
            !isOneOf(evidence, {"operator", "scripted_action", "video_timestamp", "unknown"}) ||
            !isOneOf(confidence, {"high", "medium", "low", "unknown"})) {
            return fail(error, "ground-truth context contains an unsupported value");
        }
        if (interval.motion != GroundTruthMotion::Unknown) {
            knownIntervals++;
            if (occupancy == "unknown" || environment == "unknown" ||
                evidence == "unknown" || confidence == "unknown") {
                return fail(error, "known motion interval has incomplete context/evidence");
            }
        }
        scenario.timeline.push_back(interval);
        expectedStart = interval.endMs;
    }
    if (knownIntervals == 0) {
        return fail(error, "ground truth contains no known motion interval");
    }
    scenario.durationMs = expectedStart;

    JsonVariantConst acceptanceValue = root["acceptance"];
    if (!acceptanceValue.is<JsonObjectConst>() ||
        !parseAcceptance(
            acceptanceValue.as<JsonObjectConst>(),
            scenario.acceptance,
            error)) {
        return false;
    }
    return true;
}

bool evaluateReplay(const CsiScenario& scenario,
                    const std::vector<ReplayObservation>& observations,
                    ReplayMetrics& metrics,
                    std::string& error) {
    metrics = {};
    error.clear();
    if (scenario.timeline.empty() || scenario.durationMs == 0) {
        return fail(error, "scenario has no evaluable timeline");
    }
    if (observations.empty() || observations.front().relativeMs != 0) {
        return fail(error, "replay observations must start at t=0");
    }
    if (observations.back().relativeMs >= scenario.durationMs ||
        observations.back().relativeMs + 1u != scenario.durationMs) {
        return fail(error, "scenario duration does not match the last replay frame");
    }
    for (size_t index = 0; index < observations.size(); ++index) {
        if (index > 0 && observations[index].relativeMs < observations[index - 1].relativeMs) {
            return fail(error, "replay observation time is not monotonic");
        }
        if (observations[index].lastEvidenceMs > observations[index].relativeMs) {
            return fail(error, "replay observation evidence time is in the future");
        }
        const uint8_t stateIndex = static_cast<uint8_t>(observations[index].state);
        if (stateIndex >= metrics.stateFrames.size()) {
            return fail(error, "replay observation contains an invalid detector state");
        }
        metrics.stateFrames[stateIndex]++;
    }

    std::vector<uint32_t> intervalSamples(scenario.timeline.size(), 0);
    for (const auto& observation : observations) {
        const size_t intervalIndex = intervalAt(scenario.timeline, observation.relativeMs);
        if (intervalIndex >= scenario.timeline.size()) {
            return fail(error, "observation falls outside ground-truth timeline");
        }
        intervalSamples[intervalIndex]++;
        const GroundTruthMotion truth = scenario.timeline[intervalIndex].motion;
        if (truth == GroundTruthMotion::Unknown) {
            metrics.unknownFrames++;
        } else {
            if (!decisionValidAt(observation, observation.relativeMs)) {
                metrics.invalidDecisionFrames++;
            }
            if (truth == GroundTruthMotion::Present) {
                if (observation.motion) {
                    metrics.truePositiveFrames++;
                } else {
                    metrics.falseNegativeFrames++;
                }
            } else if (observation.motion) {
                metrics.falsePositiveFrames++;
            } else {
                metrics.trueNegativeFrames++;
            }
        }
    }
    for (size_t index = 0; index < scenario.timeline.size(); ++index) {
        if (scenario.timeline[index].motion != GroundTruthMotion::Unknown &&
            intervalSamples[index] == 0) {
            return fail(error, "known ground-truth interval contains no replay frame");
        }
    }

    uint32_t currentFalsePositiveRun = 0;
    uint32_t currentFalseNegativeRun = 0;
    uint32_t currentInvalidDecisionRun = 0;
    size_t timelineIndex = 0;
    for (size_t index = 0; index < observations.size(); ++index) {
        uint32_t segmentStart = observations[index].relativeMs;
        const uint32_t segmentEnd = index + 1 < observations.size()
                                        ? observations[index + 1].relativeMs
                                        : scenario.durationMs;
        const uint64_t staleStartCandidate =
            static_cast<uint64_t>(observations[index].lastEvidenceMs) +
            CSI_MOTION_STALE_AFTER_MS + 1u;
        const uint32_t staleStart = staleStartCandidate < segmentEnd
                                        ? static_cast<uint32_t>(staleStartCandidate)
                                        : segmentEnd;
        while (timelineIndex < scenario.timeline.size() &&
               scenario.timeline[timelineIndex].endMs <= segmentStart) {
            timelineIndex++;
        }
        size_t localTimelineIndex = timelineIndex;
        while (segmentStart < segmentEnd && localTimelineIndex < scenario.timeline.size()) {
            const auto& interval = scenario.timeline[localTimelineIndex];
            uint32_t overlapEnd = std::min(segmentEnd, interval.endMs);
            const bool stale = segmentStart >= staleStart;
            if (!stale) {
                overlapEnd = std::min(overlapEnd, staleStart);
            }
            const uint32_t duration = overlapEnd - segmentStart;
            if (interval.motion == GroundTruthMotion::Unknown) {
                metrics.unknownMs += duration;
                currentFalsePositiveRun = 0;
                currentFalseNegativeRun = 0;
                currentInvalidDecisionRun = 0;
            } else {
                const bool decisionValid = decisionValidAt(
                    observations[index],
                    segmentStart);
                if (!decisionValid) {
                    metrics.invalidDecisionMs += duration;
                    currentInvalidDecisionRun += duration;
                    metrics.maxInvalidDecisionRunMs = std::max(
                        metrics.maxInvalidDecisionRunMs,
                        currentInvalidDecisionRun);
                } else {
                    currentInvalidDecisionRun = 0;
                }
                if (interval.motion == GroundTruthMotion::Present) {
                    currentFalsePositiveRun = 0;
                    if (observations[index].motion) {
                        metrics.truePositiveMs += duration;
                        currentFalseNegativeRun = 0;
                    } else {
                        metrics.falseNegativeMs += duration;
                        currentFalseNegativeRun += duration;
                        metrics.maxFalseNegativeRunMs = std::max(
                            metrics.maxFalseNegativeRunMs,
                            currentFalseNegativeRun);
                    }
                } else {
                    currentFalseNegativeRun = 0;
                    if (observations[index].motion) {
                        metrics.falsePositiveMs += duration;
                        currentFalsePositiveRun += duration;
                        metrics.maxFalsePositiveRunMs = std::max(
                            metrics.maxFalsePositiveRunMs,
                            currentFalsePositiveRun);
                    } else {
                        metrics.trueNegativeMs += duration;
                        currentFalsePositiveRun = 0;
                    }
                }
            }
            segmentStart = overlapEnd;
            if (segmentStart >= interval.endMs) {
                localTimelineIndex++;
            }
        }
    }

    const auto windows = motionWindows(scenario);
    metrics.motionIntervalCount = static_cast<uint32_t>(windows.size());
    for (const auto& window : windows) {
        uint32_t detectionMs = 0;
        if (!firstPrediction(
                observations,
                window.startMs,
                window.endMs,
                true,
                detectionMs)) {
            metrics.missedMotionIntervals++;
        } else {
            metrics.detectedMotionIntervals++;
            metrics.maxDetectionLatencyMs = std::max(
                metrics.maxDetectionLatencyMs,
                detectionMs - window.startMs);
            const PredictionRun dropout = measurePredictionRuns(
                observations,
                scenario.durationMs,
                detectionMs,
                window.endMs,
                false);
            metrics.motionDropoutCount += dropout.count;
            metrics.maxMotionDropoutMs = std::max(
                metrics.maxMotionDropoutMs,
                dropout.longestMs);
        }

        MotionWindow noneWindow;
        if (!followingKnownNoneWindow(scenario, window, noneWindow)) {
            continue;
        }
        metrics.clearTransitionCount++;
        uint32_t clearMs = 0;
        if (!firstPrediction(
                observations,
                noneWindow.startMs,
                noneWindow.endMs,
                false,
                clearMs)) {
            metrics.unclearedTransitions++;
        } else {
            metrics.clearedTransitions++;
            metrics.maxClearLatencyMs = std::max(
                metrics.maxClearLatencyMs,
                clearMs - noneWindow.startMs);
        }
    }
    metrics.observationCount = static_cast<uint32_t>(observations.size());
    return true;
}

bool evaluateAcceptance(const ReplayMetrics& metrics,
                        const ReplayAcceptance& acceptance,
                        std::vector<std::string>& violations) {
    violations.clear();
    if (metrics.falsePositiveMs > acceptance.maxFalsePositiveMs) {
        appendViolation(
            violations,
            "false_positive_ms",
            metrics.falsePositiveMs,
            acceptance.maxFalsePositiveMs);
    }
    if (metrics.falseNegativeMs > acceptance.maxFalseNegativeMs) {
        appendViolation(
            violations,
            "false_negative_ms",
            metrics.falseNegativeMs,
            acceptance.maxFalseNegativeMs);
    }
    if (metrics.invalidDecisionMs > acceptance.maxInvalidDecisionMs) {
        appendViolation(
            violations,
            "invalid_decision_ms",
            metrics.invalidDecisionMs,
            acceptance.maxInvalidDecisionMs);
    }
    if (metrics.maxDetectionLatencyMs > acceptance.maxDetectionLatencyMs) {
        appendViolation(
            violations,
            "max_detection_latency_ms",
            metrics.maxDetectionLatencyMs,
            acceptance.maxDetectionLatencyMs);
    }
    if (metrics.maxClearLatencyMs > acceptance.maxClearLatencyMs) {
        appendViolation(
            violations,
            "max_clear_latency_ms",
            metrics.maxClearLatencyMs,
            acceptance.maxClearLatencyMs);
    }
    if (metrics.maxMotionDropoutMs > acceptance.maxMotionDropoutMs) {
        appendViolation(
            violations,
            "max_motion_dropout_ms",
            metrics.maxMotionDropoutMs,
            acceptance.maxMotionDropoutMs);
    }
    if (metrics.missedMotionIntervals > acceptance.maxMissedMotionIntervals) {
        appendViolation(
            violations,
            "missed_motion_intervals",
            metrics.missedMotionIntervals,
            acceptance.maxMissedMotionIntervals);
    }
    if (metrics.unclearedTransitions > acceptance.maxUnclearedTransitions) {
        appendViolation(
            violations,
            "uncleared_transitions",
            metrics.unclearedTransitions,
            acceptance.maxUnclearedTransitions);
    }
    return violations.empty();
}

std::string sha256Hex(const uint8_t* data, size_t size) {
    Sha256 sha;
    sha.update(data, size);
    const auto digest = sha.finish();
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (uint8_t byte : digest) {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}

bool runFixtureDirectory(const std::string& fixtureDirectory,
                         FixtureRunReport& report,
                         std::string& error) {
    report = {};
    error.clear();
    const std::filesystem::path directory(fixtureDirectory);
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec) {
        return fail(error, "fixture path is not a directory: " + directory.string());
    }
    const std::string expectedFixtureId = directory.filename().string();
    const auto capturePath = directory / kCaptureFileName;
    const auto scenarioPath = directory / kScenarioFileName;
    if (!std::filesystem::is_regular_file(capturePath, ec) || ec ||
        !std::filesystem::is_regular_file(scenarioPath, ec) || ec) {
        return fail(error, "fixture must contain frames.mhcf and scenario.json");
    }
    std::filesystem::directory_iterator fixtureIterator(directory, ec);
    if (ec) {
        return fail(error, "cannot enumerate fixture directory: " + ec.message());
    }
    const std::filesystem::directory_iterator iteratorEnd;
    for (; fixtureIterator != iteratorEnd; fixtureIterator.increment(ec)) {
        if (ec) {
            return fail(error, "cannot enumerate fixture directory: " + ec.message());
        }
        const auto& entry = *fixtureIterator;
        const std::string name = entry.path().filename().string();
        if (name != kCaptureFileName && name != kScenarioFileName) {
            return fail(error, "unexpected file in promoted fixture: " + name);
        }
    }
    if (ec) {
        return fail(error, "cannot enumerate fixture directory: " + ec.message());
    }

    std::string scenarioJson;
    if (!readTextFile(scenarioPath, kMaxScenarioBytes, scenarioJson, error)) {
        return false;
    }
    CsiScenario scenario;
    if (!parseScenarioJson(
            scenarioJson,
            expectedFixtureId,
            ScenarioSourcePolicy::RealDeviceOnly,
            scenario,
            error)) {
        return false;
    }
    std::vector<uint8_t> captureBytes;
    if (!readBinaryFile(capturePath, captureBytes, error)) {
        return false;
    }
    const std::string digest = "sha256:" + sha256Hex(captureBytes.data(), captureBytes.size());
    if (digest != scenario.captureSha256) {
        return fail(error, "scenario capture_sha256 does not match frames.mhcf");
    }

    CsiCaptureDecoder decoder;
    if (!decoder.open(captureBytes.data(), captureBytes.size())) {
        return fail(error, std::string("invalid MHCF: ") + toString(decoder.error()));
    }
    CsiBandMotionDetector detector;
    if (!detector.begin()) {
        return fail(error, "production detector storage allocation failed");
    }
    detector.configure(scenario.detectorConfig);

    struct ObserverContext {
        bool hasOrigin = false;
        bool valid = true;
        uint32_t originMs = 0;
        uint32_t previousRelativeMs = 0;
        std::vector<ReplayObservation> observations;
    } context;
    context.observations.reserve(decoder.frameCount());
    const auto observer = [](uint32_t,
                             const CsiCaptureFrame& frame,
                             const CsiMotionSnapshot& snapshot,
                             void* opaque) {
        auto* state = static_cast<ObserverContext*>(opaque);
        if (!state->hasOrigin) {
            state->hasOrigin = true;
            state->originMs = frame.processNowMs;
        }
        const uint32_t relativeMs = frame.processNowMs - state->originMs;
        if (relativeMs >= kUint32HalfRange ||
            (!state->observations.empty() && relativeMs < state->previousRelativeMs)) {
            state->valid = false;
            return;
        }
        uint32_t lastEvidenceMs = 0;
        bool decisionValid = snapshot.decisionValid && snapshot.hasFrame;
        if (snapshot.hasFrame) {
            lastEvidenceMs = snapshot.lastFrameMs - state->originMs;
            if (lastEvidenceMs >= kUint32HalfRange || lastEvidenceMs > relativeMs) {
                state->valid = false;
                return;
            }
        }
        state->previousRelativeMs = relativeMs;
        state->observations.push_back({
            relativeMs,
            snapshot.motion,
            snapshot.state,
            decisionValid,
            lastEvidenceMs,
        });
    };
    CsiCaptureReplayResult replayResult;
    const bool replayed = replayCsiCapture(
        decoder,
        detector,
        replayResult,
        observer,
        &context);
    detector.end();
    if (!replayed || !context.valid || context.observations.size() != decoder.frameCount()) {
        return fail(error, "exact production detector replay did not consume every frame");
    }

    ReplayMetrics metrics;
    if (!evaluateReplay(scenario, context.observations, metrics, error)) {
        return false;
    }
    report.fixtureId = scenario.fixtureId;
    report.frameCount = decoder.frameCount();
    report.metrics = metrics;
    evaluateAcceptance(metrics, scenario.acceptance, report.acceptanceViolations);
    return true;
}

bool listFixtureDirectories(const std::string& root,
                            std::vector<std::string>& fixtureDirectories,
                            std::string& error) {
    fixtureDirectories.clear();
    error.clear();
    const std::filesystem::path rootPath(root);
    std::error_code ec;
    if (!std::filesystem::is_directory(rootPath, ec) || ec) {
        return fail(error, "CSI fixture root is not a directory: " + rootPath.string());
    }
    std::filesystem::directory_iterator rootIterator(rootPath, ec);
    if (ec) {
        return fail(error, "cannot enumerate CSI fixture root: " + ec.message());
    }
    const std::filesystem::directory_iterator iteratorEnd;
    for (; rootIterator != iteratorEnd; rootIterator.increment(ec)) {
        if (ec) {
            return fail(error, "cannot enumerate CSI fixture root: " + ec.message());
        }
        const auto& entry = *rootIterator;
        const std::string name = entry.path().filename().string();
        const bool directory = entry.is_directory(ec);
        if (ec) {
            return fail(error, "cannot inspect CSI fixture root entry: " + ec.message());
        }
        if (directory) {
            fixtureDirectories.push_back(entry.path().string());
            continue;
        }
        if (name == "README.md" || name == ".gitkeep") {
            continue;
        }
        return fail(error, "unexpected non-directory in CSI fixture root: " + name);
    }
    if (ec) {
        return fail(error, "cannot enumerate CSI fixture root: " + ec.message());
    }
    std::sort(fixtureDirectories.begin(), fixtureDirectories.end());
    if (fixtureDirectories.empty()) {
        return fail(error, "CSI fixture corpus is empty");
    }
    return true;
}

} // namespace NATIVE_CAPTURE
} // namespace CSI
} // namespace WIFISENSING
