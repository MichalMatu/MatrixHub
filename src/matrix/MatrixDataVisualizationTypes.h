#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MATRIX {

constexpr uint8_t kMatrixDataVizWidth = 8;
constexpr uint8_t kMatrixDataVizHeight = 8;
constexpr uint8_t kMatrixDataVizPixelCount = kMatrixDataVizWidth * kMatrixDataVizHeight;
constexpr size_t kMatrixDataVizDeviceIdCapacity = 18;  // BLE MAC including terminator

enum class MatrixBackgroundMode : uint8_t {
    Effects = 0,
    DataVisualization = 1,
    Off = 2,
};

enum class MatrixDataSource : uint8_t {
    Scd4x = 0,
    BleThermometer = 1,
    WifiRssi = 2,
    WifiCsi = 3,
};

enum class MatrixDataMetric : uint8_t {
    Co2 = 0,
    Temperature = 1,
    Humidity = 2,
    Rssi = 3,
    SignalQuality = 4,
    CsiMotion = 5,
};

enum class MatrixDataVizMode : uint8_t {
    Gauge = 0,
    CenterRipple = 1,
    Heatmap = 2,
    Trend = 3,
    SpectrumBars = 4,
    PerimeterMeter = 5,
    Pulse = 6,
};

enum class MatrixDataStaleBehavior : uint8_t {
    Dim = 0,
    Gray = 1,
    Blank = 2,
};

enum class MatrixDataVisualizationReason : uint8_t {
    Ok = 0,
    Disabled = 1,
    NoService = 2,
    NoSample = 3,
    Stale = 4,
    WifiInactive = 5,
    NoBleDevice = 6,
    NoScd4xReading = 7,
    NoCsiPacket = 8,
};

struct MatrixDataVisualizationConfig {
    bool enabled = false;
    uint8_t source = static_cast<uint8_t>(MatrixDataSource::Scd4x);
    uint8_t metric = static_cast<uint8_t>(MatrixDataMetric::Co2);
    uint8_t mode = static_cast<uint8_t>(MatrixDataVizMode::Gauge);
    float minValue = 0.0f;
    float maxValue = 100.0f;
    uint32_t colorMin = 0x0040FF;
    uint32_t colorMid = 0x00FF80;
    uint32_t colorMax = 0xFF3000;
    uint8_t brightnessMin = 12;
    uint8_t brightnessMax = 180;
    uint8_t smoothing = 50;
    uint8_t staleBehavior = static_cast<uint8_t>(MatrixDataStaleBehavior::Dim);
    char deviceId[kMatrixDataVizDeviceIdCapacity] = {};
};

struct MatrixDataVisualizationInput {
    bool valid = false;
    bool stale = false;
    bool calibrationReady = false;
    bool needsCalibration = false;
    uint8_t reason = static_cast<uint8_t>(MatrixDataVisualizationReason::Disabled);
    float value = 0.0f;
    float secondary = 0.0f;
    uint32_t timestampMs = 0;
    uint8_t bins[kMatrixDataVizPixelCount] = {};
    uint8_t binCount = 0;
};

struct MatrixDataVisualizationStatusSnapshot {
    bool active = false;
    MatrixDataVisualizationConfig config{};
    MatrixDataVisualizationInput input{};
    uint32_t updatedAtMs = 0;
};

constexpr uint8_t kMatrixBackgroundModeMax =
    static_cast<uint8_t>(MatrixBackgroundMode::Off);
constexpr uint8_t kMatrixDataSourceMax =
    static_cast<uint8_t>(MatrixDataSource::WifiCsi);
constexpr uint8_t kMatrixDataMetricMax =
    static_cast<uint8_t>(MatrixDataMetric::CsiMotion);
constexpr uint8_t kMatrixDataVizModeMax =
    static_cast<uint8_t>(MatrixDataVizMode::Pulse);
constexpr uint8_t kMatrixDataStaleBehaviorMax =
    static_cast<uint8_t>(MatrixDataStaleBehavior::Blank);

inline uint8_t normalizeMatrixBackgroundMode(uint8_t value) {
    return value <= kMatrixBackgroundModeMax
        ? value
        : static_cast<uint8_t>(MatrixBackgroundMode::Off);
}

inline uint8_t normalizeMatrixDataSource(uint8_t value) {
    return value <= kMatrixDataSourceMax
        ? value
        : static_cast<uint8_t>(MatrixDataSource::Scd4x);
}

inline uint8_t normalizeMatrixDataMetric(uint8_t value) {
    return value <= kMatrixDataMetricMax
        ? value
        : static_cast<uint8_t>(MatrixDataMetric::Co2);
}

inline uint8_t normalizeMatrixDataVizMode(uint8_t value) {
    return value <= kMatrixDataVizModeMax
        ? value
        : static_cast<uint8_t>(MatrixDataVizMode::Gauge);
}

inline uint8_t normalizeMatrixDataStaleBehavior(uint8_t value) {
    return value <= kMatrixDataStaleBehaviorMax
        ? value
        : static_cast<uint8_t>(MatrixDataStaleBehavior::Dim);
}

inline const char* matrixDataVisualizationReasonToString(uint8_t value) {
    switch (static_cast<MatrixDataVisualizationReason>(value)) {
        case MatrixDataVisualizationReason::Ok:
            return "ok";
        case MatrixDataVisualizationReason::Disabled:
            return "disabled";
        case MatrixDataVisualizationReason::NoService:
            return "no_service";
        case MatrixDataVisualizationReason::NoSample:
            return "no_sample";
        case MatrixDataVisualizationReason::Stale:
            return "stale";
        case MatrixDataVisualizationReason::WifiInactive:
            return "wifi_inactive";
        case MatrixDataVisualizationReason::NoBleDevice:
            return "no_ble_device";
        case MatrixDataVisualizationReason::NoScd4xReading:
            return "no_scd4x_reading";
        case MatrixDataVisualizationReason::NoCsiPacket:
            return "no_csi_packet";
        default:
            return "disabled";
    }
}

inline bool isMatrixMetricValidForSource(uint8_t source, uint8_t metric) {
    const auto normalizedSource = static_cast<MatrixDataSource>(normalizeMatrixDataSource(source));
    const auto normalizedMetric = static_cast<MatrixDataMetric>(normalizeMatrixDataMetric(metric));

    switch (normalizedSource) {
        case MatrixDataSource::Scd4x:
            return normalizedMetric == MatrixDataMetric::Co2 ||
                   normalizedMetric == MatrixDataMetric::Temperature ||
                   normalizedMetric == MatrixDataMetric::Humidity;
        case MatrixDataSource::BleThermometer:
            return normalizedMetric == MatrixDataMetric::Temperature ||
                   normalizedMetric == MatrixDataMetric::Humidity ||
                   normalizedMetric == MatrixDataMetric::Rssi;
        case MatrixDataSource::WifiRssi:
            return normalizedMetric == MatrixDataMetric::Rssi ||
                   normalizedMetric == MatrixDataMetric::SignalQuality;
        case MatrixDataSource::WifiCsi:
            return normalizedMetric == MatrixDataMetric::CsiMotion;
        default:
            return false;
    }
}

inline uint8_t defaultMetricForSource(uint8_t source) {
    switch (static_cast<MatrixDataSource>(normalizeMatrixDataSource(source))) {
        case MatrixDataSource::BleThermometer:
            return static_cast<uint8_t>(MatrixDataMetric::Temperature);
        case MatrixDataSource::WifiRssi:
            return static_cast<uint8_t>(MatrixDataMetric::SignalQuality);
        case MatrixDataSource::WifiCsi:
            return static_cast<uint8_t>(MatrixDataMetric::CsiMotion);
        case MatrixDataSource::Scd4x:
        default:
            return static_cast<uint8_t>(MatrixDataMetric::Co2);
    }
}

inline bool isMatrixModeValidForSource(uint8_t source, uint8_t mode) {
    const auto normalizedSource = static_cast<MatrixDataSource>(normalizeMatrixDataSource(source));
    const auto normalizedMode = static_cast<MatrixDataVizMode>(normalizeMatrixDataVizMode(mode));

    if (normalizedSource == MatrixDataSource::WifiCsi) {
        return normalizedMode == MatrixDataVizMode::Heatmap ||
               normalizedMode == MatrixDataVizMode::SpectrumBars ||
               normalizedMode == MatrixDataVizMode::Pulse;
    }

    return normalizedMode == MatrixDataVizMode::Gauge ||
           normalizedMode == MatrixDataVizMode::Trend ||
           normalizedMode == MatrixDataVizMode::PerimeterMeter ||
           normalizedMode == MatrixDataVizMode::Pulse;
}

inline uint8_t defaultModeForSource(uint8_t source) {
    return static_cast<MatrixDataSource>(normalizeMatrixDataSource(source)) == MatrixDataSource::WifiCsi
        ? static_cast<uint8_t>(MatrixDataVizMode::Heatmap)
        : static_cast<uint8_t>(MatrixDataVizMode::Gauge);
}

inline uint32_t normalizeMatrixDataColor(uint32_t value) {
    return value & 0x00FFFFFFu;
}

inline void normalizeMatrixDataVisualizationConfig(MatrixDataVisualizationConfig& config) {
    config.source = normalizeMatrixDataSource(config.source);
    config.metric = normalizeMatrixDataMetric(config.metric);
    config.mode = normalizeMatrixDataVizMode(config.mode);
    config.staleBehavior = normalizeMatrixDataStaleBehavior(config.staleBehavior);
    config.colorMin = normalizeMatrixDataColor(config.colorMin);
    config.colorMid = normalizeMatrixDataColor(config.colorMid);
    config.colorMax = normalizeMatrixDataColor(config.colorMax);

    if (!isMatrixMetricValidForSource(config.source, config.metric)) {
        config.metric = defaultMetricForSource(config.source);
    }
    if (!isMatrixModeValidForSource(config.source, config.mode)) {
        config.mode = defaultModeForSource(config.source);
    }

    if (config.source == static_cast<uint8_t>(MatrixDataSource::WifiCsi)) {
        config.minValue = 0.0f;
        config.maxValue = 100.0f;
    } else if (config.maxValue <= config.minValue) {
        config.maxValue = config.minValue + 1.0f;
    }

    if (config.brightnessMax < config.brightnessMin) {
        const uint8_t tmp = config.brightnessMax;
        config.brightnessMax = config.brightnessMin;
        config.brightnessMin = tmp;
    }
    if (config.brightnessMax == 0) {
        config.brightnessMax = 1;
    }
}

inline void copyMatrixDataDeviceId(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    std::strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

}  // namespace MATRIX
