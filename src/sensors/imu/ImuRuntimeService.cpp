#include "ImuRuntimeService.h"

#include "ImuMath.h"
#include "ImuService.h"
#include "../../alarms/AlarmService.h"
#include "../../alarms/types/AlarmInputData.h"
#include "../../config/App.h"
#include "../../config/json/ImuConfigJson.h"
#include "../../core/config/ConfigManager.h"
#include "../../system/logging/Logging.h"

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "ImuRuntime"

namespace IMU {

namespace {
constexpr uint32_t kMetricUpdateIntervalMs = 100;
constexpr uint16_t kCalibrationSamples = 50;
constexpr uint32_t kCalibrationSampleDelayMs = 10;
constexpr float kCalibrationMaxAccelVariance = 0.0035f;

bool readFreshSample(ImuService* service, ImuSample& sample) {
    if (!service) {
        sample = {};
        return false;
    }
    return service->getCachedSample(sample);
}

}  // namespace

ImuRuntimeService::ImuRuntimeService(FS* fs,
                                     ImuService* imuService,
                                     ImuManager* imuManager,
                                     ALARMS::AlarmService* alarmService)
    : RtcStatefulService(&RTC::ConfigStore::imu),
      _fs(fs),
      _imuService(imuService),
      _imuManager(imuManager),
      _alarmService(alarmService) {
    publishBaselineFromSettings(_state);
}

void ImuRuntimeService::begin() {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            applyConsumerState();
            return persistConfig();
        },
        false);
    applyConsumerState();
}

void ImuRuntimeService::tick() {
    if (_imuManager) {
        _imuManager->tick();
    }
    applyConsumerState();

    const uint32_t now = millis();
    if (_lastMetricUpdateMs != 0 && now - _lastMetricUpdateMs < kMetricUpdateIntervalMs) {
        return;
    }
    _lastMetricUpdateMs = now;
    (void)updateMetricsFromCache(now);
    updateAlarmDetector(now);
}

ImuMetrics ImuRuntimeService::getMetrics() const {
    ImuMetrics copy;
    portENTER_CRITICAL(&_metricsMux);
    copy = _metrics;
    portEXIT_CRITICAL(&_metricsMux);
    return copy;
}

ManagerStatus ImuRuntimeService::getManagerStatus() const {
    return _imuManager ? _imuManager->getStatus() : ManagerStatus{};
}

ImuAlarmStatus ImuRuntimeService::getAlarmStatus() const {
    ImuAlarmStatus copy;
    portENTER_CRITICAL(&_alarmStatusMux);
    copy = _alarmStatus;
    portEXIT_CRITICAL(&_alarmStatusMux);
    return copy;
}

void ImuRuntimeService::setAlarmService(ALARMS::AlarmService* alarmService) {
    _alarmService = alarmService;
}

void ImuRuntimeService::applyConsumerState() {
    if (!_imuManager) {
        return;
    }

    RTC::ImuData settings{};
    bool loaded = false;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        settings = cfg.imu;
        loaded = true;
    });
    if (!loaded) {
        settings = getCachedStateCopy();
    }

    _imuManager->setConsumerActive(Consumer::UiMonitor, settings.uiMonitorEnabled);
    _imuManager->setConsumerActive(Consumer::Alarm, settings.alarmMonitorEnabled);
    publishBaselineFromSettings(settings);
}

ImuAlarmConfig ImuRuntimeService::alarmConfigFromSettings(const RTC::ImuData& settings) {
    ImuAlarmConfig config;
    config.enabled = settings.alarmMonitorEnabled;
    config.baselineValid = settings.orientationBaselineValid;
    config.tiltThresholdDeg = settings.tiltThresholdDeg;
    config.tiltHysteresisDeg = settings.tiltHysteresisDeg;
    config.tiltHoldMs = settings.tiltHoldMs;
    config.tiltClearHoldMs = settings.tiltClearHoldMs;
    config.accelDeltaThresholdG = settings.accelDeltaThresholdG;
    return config;
}

void ImuRuntimeService::publishBaselineFromSettings(const RTC::ImuData& settings) {
    portENTER_CRITICAL(&_metricsMux);
    _metrics.baselineValid = settings.orientationBaselineValid;
    _metrics.baseline = {
        settings.orientationBaselineX,
        settings.orientationBaselineY,
        settings.orientationBaselineZ
    };
    portEXIT_CRITICAL(&_metricsMux);
}

bool ImuRuntimeService::updateMetricsFromCache(uint32_t nowMs) {
    ImuSample sample;
    const bool fresh = readFreshSample(_imuService, sample);

    RTC::ImuData settings{};
    bool loaded = false;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        settings = cfg.imu;
        loaded = true;
    });
    if (!loaded) {
        settings = getCachedStateCopy();
    }

    ImuMetrics next = getMetrics();
    next.sample = sample;
    next.sampleFresh = fresh;
    next.sampleTimestampKnown = sample.valid || sample.timestampMs != 0;
    next.sampleAgeMs = next.sampleTimestampKnown ? MATH::elapsedMs(nowMs, sample.timestampMs) : 0;
    next.baselineValid = settings.orientationBaselineValid;
    next.baseline = {
        settings.orientationBaselineX,
        settings.orientationBaselineY,
        settings.orientationBaselineZ
    };

    if (fresh) {
        next.accelMagnitudeG = MATH::magnitude(sample.ax, sample.ay, sample.az);
        next.accelDeltaG = fabsf(next.accelMagnitudeG - 1.0f);
        next.gyroMagnitudeDps = MATH::magnitude(sample.gx, sample.gy, sample.gz);
        if (settings.orientationBaselineValid &&
            MATH::isAccelMagnitudeStable(next.accelMagnitudeG)) {
            const ImuVector3 current{sample.ax, sample.ay, sample.az};
            next.tiltDeg = MATH::tiltDegrees(next.baseline, current);
        } else {
            next.tiltDeg = NAN;
        }
    }

    portENTER_CRITICAL(&_metricsMux);
    _metrics = next;
    portEXIT_CRITICAL(&_metricsMux);
    return fresh;
}

void ImuRuntimeService::updateAlarmDetector(uint32_t nowMs) {
    RTC::ImuData settings{};
    bool loaded = false;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        settings = cfg.imu;
        loaded = true;
    });
    if (!loaded) {
        settings = getCachedStateCopy();
    }

    const ImuAlarmConfig config = alarmConfigFromSettings(settings);
    const ImuMetrics metrics = getMetrics();
    const ImuAlarmStatus status = _alarmDetector.update(config, metrics, nowMs);

    portENTER_CRITICAL(&_alarmStatusMux);
    _alarmStatus = status;
    portEXIT_CRITICAL(&_alarmStatusMux);

    publishAlarmValue(status);
}

void ImuRuntimeService::publishAlarmValue(const ImuAlarmStatus& status) {
    if (!_alarmService) {
        return;
    }

    ALARMS::AlarmInputData input;
    input.imuTamper = status.triggerValue;
    _alarmService->submitInput(input);
}

OrientationCalibrationResult ImuRuntimeService::calibrateOrientation() {
    OrientationCalibrationResult result;

    if (!_imuManager || !_imuService) {
        result.status = OrientationCalibrationStatus::NotRunning;
        return result;
    }

    const RTC::ImuData settingsBefore = getCachedStateCopy();
    const bool restoreUiConsumer = !settingsBefore.uiMonitorEnabled;
    _imuManager->setConsumerActive(Consumer::UiMonitor, true);

    ImuVector3 sum{};
    float mean = 0.0f;
    float m2 = 0.0f;
    uint16_t count = 0;

    for (uint16_t i = 0; i < kCalibrationSamples; i++) {
        ImuSample sample;
        if (readFreshSample(_imuService, sample)) {
            const float mag = MATH::magnitude(sample.ax, sample.ay, sample.az);
            if (MATH::isAccelMagnitudeStable(mag)) {
                count++;
                sum.x += sample.ax;
                sum.y += sample.ay;
                sum.z += sample.az;

                const float delta = mag - mean;
                mean += delta / static_cast<float>(count);
                const float delta2 = mag - mean;
                m2 += delta * delta2;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kCalibrationSampleDelayMs));
    }

    result.sampleCount = count;
    result.accelMagnitudeMean = mean;
    result.accelMagnitudeVariance = count > 1 ? m2 / static_cast<float>(count - 1) : 0.0f;

    if (count < kCalibrationSamples / 2) {
        result.status = OrientationCalibrationStatus::NoFreshSamples;
    } else if (result.accelMagnitudeVariance > kCalibrationMaxAccelVariance) {
        result.status = OrientationCalibrationStatus::Unstable;
    } else {
        ImuVector3 average{
            sum.x / static_cast<float>(count),
            sum.y / static_cast<float>(count),
            sum.z / static_cast<float>(count)
        };
        if (!MATH::normalize(average, result.baseline)) {
            result.status = OrientationCalibrationStatus::InvalidVector;
        } else {
            StateTransactionResult tx = updateAndPropagate(
                [&](RTC::ImuData& data) {
                    data.orientationBaselineValid = true;
                    data.orientationBaselineX = result.baseline.x;
                    data.orientationBaselineY = result.baseline.y;
                    data.orientationBaselineZ = result.baseline.z;
                    data.baselineCalibratedAt = millis();
                    data.calibrationRevision++;
                    return StateUpdateResult::CHANGED;
                },
                "imu/calibrate/orientation");
            result.status = (tx.outcome == StateUpdateResult::ERROR)
                ? OrientationCalibrationStatus::InvalidVector
                : OrientationCalibrationStatus::Success;
        }
    }

    if (restoreUiConsumer) {
        _imuManager->setConsumerActive(Consumer::UiMonitor, false);
    }
    (void)updateMetricsFromCache(millis());
    return result;
}

bool ImuRuntimeService::resetOrientationBaseline() {
    StateTransactionResult tx = updateAndPropagate(
        [](RTC::ImuData& data) {
            data.orientationBaselineValid = false;
            data.orientationBaselineX = 0.0f;
            data.orientationBaselineY = 0.0f;
            data.orientationBaselineZ = 1.0f;
            data.baselineCalibratedAt = 0;
            data.calibrationRevision++;
            return StateUpdateResult::CHANGED;
        },
        "imu/reset/orientation_baseline");
    publishBaselineFromSettings(getCachedStateCopy());
    _alarmDetector.reset();
    updateAlarmDetector(millis());
    return tx.outcome != StateUpdateResult::ERROR;
}

void ImuRuntimeService::readState(RTC::ImuData& settings, JsonObject& root) {
    root[CONFIG::Keys::kUiMonitorEnabled] = settings.uiMonitorEnabled;
    root[CONFIG::Keys::kAlarmMonitorEnabled] = settings.alarmMonitorEnabled;
    root[CONFIG::Keys::kOrientationBaselineValid] = settings.orientationBaselineValid;
    JsonObject baseline = root[CONFIG::Keys::kOrientationBaseline].to<JsonObject>();
    baseline[CONFIG::Keys::kX] = settings.orientationBaselineX;
    baseline[CONFIG::Keys::kY] = settings.orientationBaselineY;
    baseline[CONFIG::Keys::kZ] = settings.orientationBaselineZ;
    root[CONFIG::Keys::kBaselineCalibratedAt] = settings.baselineCalibratedAt;
    root[CONFIG::Keys::kCalibrationRevision] = settings.calibrationRevision;
    root[CONFIG::Keys::kTiltThresholdDeg] = settings.tiltThresholdDeg;
    root[CONFIG::Keys::kTiltHysteresisDeg] = settings.tiltHysteresisDeg;
    root[CONFIG::Keys::kTiltHoldMs] = settings.tiltHoldMs;
    root[CONFIG::Keys::kTiltClearHoldMs] = settings.tiltClearHoldMs;
    root[CONFIG::Keys::kAccelDeltaThresholdG] = settings.accelDeltaThresholdG;
}

StateUpdateResult ImuRuntimeService::updateState(
    JsonObject& jsonObject,
    RTC::ImuData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::ImuData next = settings;
    CONFIG::JSON::deserializeImu(jsonObject, next);

    if (memcmp(&settings, &next, sizeof(RTC::ImuData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = next;
    return StateUpdateResult::CHANGED;
}

StateHandlerResult ImuRuntimeService::persistConfig() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist IMU settings");
        return StateHandlerResult::failure("config/save_failed");
    }
    return StateHandlerResult::success();
}

}  // namespace IMU
