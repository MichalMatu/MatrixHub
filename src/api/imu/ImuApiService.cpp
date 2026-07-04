#include "ImuApiService.h"

#include "../../config/json/ConfigKeys.h"
#include "../../sensors/imu/ImuMath.h"
#include "../../sensors/imu/ImuRuntimeService.h"
#include "../../system/utils/json/JsonResponseWriter.h"

#include <utils/ResponseUtils.h>

#include <cmath>

namespace API {

namespace {
constexpr const char* kSettingsPath = "/api/imu/settings";
constexpr const char* kStatusPath = "/api/imu/status";
constexpr const char* kCalibrateOrientationPath = "/api/imu/calibrate/orientation";
constexpr const char* kResetOrientationBaselinePath = "/api/imu/reset/orientation-baseline";
constexpr const char* kUnavailableError = "imu/unavailable";

constexpr uint32_t consumerBit(IMU::Consumer consumer) {
    return 1u << static_cast<uint8_t>(consumer);
}

void writeConsumer(Utils::JsonResponseWriter& w,
                   const char* key,
                   IMU::Consumer consumer,
                   uint32_t desiredMask,
                   uint32_t runningMask) {
    const uint32_t bit = consumerBit(consumer);
    w.raw(","); w.string(key); w.raw(":{");
    w.string("desired"); w.raw(":"); w.value((desiredMask & bit) != 0);
    w.raw(","); w.key(CONFIG::Keys::kRunning); w.value((runningMask & bit) != 0);
    w.raw("}");
}

void writeVector(Utils::JsonResponseWriter& w, const IMU::ImuVector3& v) {
    w.raw("{");
    w.key(CONFIG::Keys::kX); w.value(v.x, 4);
    w.raw(","); w.key(CONFIG::Keys::kY); w.value(v.y, 4);
    w.raw(","); w.key(CONFIG::Keys::kZ); w.value(v.z, 4);
    w.raw("}");
}

void writeFiniteOrNull(Utils::JsonResponseWriter& w, float value, uint8_t decimals) {
    if (std::isfinite(value)) {
        w.value(value, decimals);
    } else {
        w.raw("null");
    }
}

}  // namespace

ImuApiService::ImuApiService(PsychicHttpServer* server,
                             SecurityManager* securityManager,
                             POWER::PowerManager* powerManager,
                             IMU::ImuRuntimeService* runtime)
    : BaseApiService(server, securityManager, powerManager, "api/imu"),
      _runtime(runtime) {
    if (_runtime) {
        _settingsEndpoint = std::make_unique<HttpEndpoint<RTC::ImuData>>(
            IMU::ImuRuntimeService::readState,
            IMU::ImuRuntimeService::updateState,
            _runtime,
            _server,
            kSettingsPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            AuthenticationPredicates::IS_AUTHENTICATED,
            nullptr,
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void ImuApiService::begin() {
    _server->on(kStatusPath, HTTP_GET, wrapAuth([this](PsychicRequest* request) {
        return handleStatus(request);
    }));

    if (_settingsEndpoint) {
        _settingsEndpoint->begin();
    }

    _server->on(kCalibrateOrientationPath, HTTP_POST, wrapAdmin([this](PsychicRequest* request) {
        return handleCalibrateOrientation(request);
    }));

    _server->on(kResetOrientationBaselinePath, HTTP_POST, wrapAdmin([this](PsychicRequest* request) {
        return handleResetOrientationBaseline(request);
    }));
}

esp_err_t ImuApiService::handleStatus(PsychicRequest* request) {
    if (!_runtime) {
        return Response::error(request, 503, kUnavailableError);
    }

    const IMU::ManagerStatus manager = _runtime->getManagerStatus();
    const IMU::ImuMetrics metrics = _runtime->getMetrics();
    const IMU::ImuAlarmStatus alarm = _runtime->getAlarmStatus();

    char desiredConsumers[128];
    char runningConsumers[128];
    IMU::ImuManager::formatConsumers(manager.desiredMask, desiredConsumers, sizeof(desiredConsumers));
    IMU::ImuManager::formatConsumers(manager.runningMask, runningConsumers, sizeof(runningConsumers));

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_FAIL;

    w.raw("{");
    w.key(CONFIG::Keys::kInitialized); w.value(manager.initialized);
    w.raw(","); w.key(CONFIG::Keys::kRunning); w.value(manager.initialized && manager.runningMask != 0);
    w.raw(","); w.key(CONFIG::Keys::kTransitionInProgress); w.value(manager.transitionInProgress);
    w.raw(","); w.key(CONFIG::Keys::kDesiredMask); w.value(static_cast<unsigned long>(manager.desiredMask));
    w.raw(","); w.key(CONFIG::Keys::kRunningMask); w.value(static_cast<unsigned long>(manager.runningMask));
    w.raw(","); w.key(CONFIG::Keys::kDesiredConsumers); w.string(desiredConsumers);
    w.raw(","); w.key(CONFIG::Keys::kRunningConsumers); w.string(runningConsumers);
    w.raw(","); w.key(CONFIG::Keys::kLastStartError);
    w.string(IMU::ImuManager::startErrorToString(manager.lastStartError));
    w.raw(","); w.key(CONFIG::Keys::kLastStartAttemptMs); w.value(static_cast<unsigned long>(manager.lastStartAttemptMs));
    w.raw(","); w.key(CONFIG::Keys::kLastStartDurationMs); w.value(static_cast<unsigned long>(manager.lastStartDurationMs));
    w.raw(","); w.key(CONFIG::Keys::kNextRetryMs); w.value(static_cast<unsigned long>(manager.nextRetryMs));
    w.raw(","); w.key(CONFIG::Keys::kRetryPending);
    w.value(manager.lastStartError == IMU::StartError::RetryPending ||
            manager.lastStartError == IMU::StartError::StartFailed);
    w.raw(","); w.key(CONFIG::Keys::kSampleFresh); w.value(metrics.sampleFresh);
    w.raw(","); w.key(CONFIG::Keys::kSampleAgeMs);
    if (metrics.sampleTimestampKnown) {
        w.value(static_cast<unsigned long>(metrics.sampleAgeMs));
    } else {
        w.raw("null");
    }
    w.raw(","); w.key(CONFIG::Keys::kLastSampleMs);
    if (metrics.sampleTimestampKnown) {
        w.value(static_cast<unsigned long>(metrics.sample.timestampMs));
    } else {
        w.raw("null");
    }

    w.raw(","); w.key(CONFIG::Keys::kConsumers);
    w.raw("{");
    w.string("airmouse_movement"); w.raw(":{");
    w.string("desired"); w.raw(":"); w.value((manager.desiredMask & consumerBit(IMU::Consumer::AirMouseMovement)) != 0);
    w.raw(","); w.key(CONFIG::Keys::kRunning); w.value((manager.runningMask & consumerBit(IMU::Consumer::AirMouseMovement)) != 0);
    w.raw("}");
    writeConsumer(w, "airmouse_click", IMU::Consumer::AirMouseClick, manager.desiredMask, manager.runningMask);
    writeConsumer(w, "auto_rotate", IMU::Consumer::AutoRotate, manager.desiredMask, manager.runningMask);
    writeConsumer(w, "alarm", IMU::Consumer::Alarm, manager.desiredMask, manager.runningMask);
    writeConsumer(w, "ui_monitor", IMU::Consumer::UiMonitor, manager.desiredMask, manager.runningMask);
    writeConsumer(w, "matrix_effects", IMU::Consumer::MatrixEffects, manager.desiredMask, manager.runningMask);
    w.raw("}");

    w.raw(","); w.key(CONFIG::Keys::kAlarm);
    w.raw("{");
    w.key(CONFIG::Keys::kEnabled); w.value(alarm.enabled);
    w.raw(","); w.key(CONFIG::Keys::kSampleFresh); w.value(alarm.sampleFresh);
    w.raw(","); w.key(CONFIG::Keys::kBaselineValid); w.value(alarm.baselineValid);
    w.raw(","); w.key(CONFIG::Keys::kTriggered); w.value(alarm.triggered);
    w.raw(","); w.key(CONFIG::Keys::kPendingTrigger); w.value(alarm.pendingTrigger);
    w.raw(","); w.key(CONFIG::Keys::kPendingClear); w.value(alarm.pendingClear);
    w.raw(","); w.key(CONFIG::Keys::kReason); w.string(IMU::MATH::alarmReasonToString(alarm.reason));
    w.raw(","); w.key(CONFIG::Keys::kTiltDeg); writeFiniteOrNull(w, alarm.currentTiltDeg, 2);
    w.raw(","); w.key(CONFIG::Keys::kAccelDeltaG); w.value(alarm.accelDeltaG, 4);
    w.raw(","); w.key(CONFIG::Keys::kTriggerValue); w.value(alarm.triggerValue, 1);
    w.raw(","); w.key(CONFIG::Keys::kTriggerHoldElapsedMs);
    w.value(static_cast<unsigned long>(alarm.triggerHoldElapsedMs));
    w.raw(","); w.key(CONFIG::Keys::kClearHoldElapsedMs);
    w.value(static_cast<unsigned long>(alarm.clearHoldElapsedMs));
    w.raw("}");

    w.raw(","); w.key(CONFIG::Keys::kMetrics);
    w.raw("{");
    w.key(CONFIG::Keys::kAccelMagnitudeG); w.value(metrics.accelMagnitudeG, 4);
    w.raw(","); w.key(CONFIG::Keys::kAccelDeltaG); w.value(metrics.accelDeltaG, 4);
    w.raw(","); w.key(CONFIG::Keys::kGyroMagnitudeDps); w.value(metrics.gyroMagnitudeDps, 2);
    w.raw(","); w.key(CONFIG::Keys::kBaselineValid); w.value(metrics.baselineValid);
    w.raw(","); w.key(CONFIG::Keys::kOrientationBaseline); writeVector(w, metrics.baseline);
    w.raw(","); w.key(CONFIG::Keys::kTiltDeg); writeFiniteOrNull(w, metrics.tiltDeg, 2);
    w.raw(","); w.key(CONFIG::Keys::kSample);
    w.raw("{");
    w.key(CONFIG::Keys::kAx); w.value(metrics.sample.ax, 4);
    w.raw(","); w.key(CONFIG::Keys::kAy); w.value(metrics.sample.ay, 4);
    w.raw(","); w.key(CONFIG::Keys::kAz); w.value(metrics.sample.az, 4);
    w.raw(","); w.key(CONFIG::Keys::kGx); w.value(metrics.sample.gx, 4);
    w.raw(","); w.key(CONFIG::Keys::kGy); w.value(metrics.sample.gy, 4);
    w.raw(","); w.key(CONFIG::Keys::kGz); w.value(metrics.sample.gz, 4);
    w.raw("}");
    w.raw("}");
    w.raw("}");

    return w.finish() ? ESP_OK : ESP_FAIL;
}

esp_err_t ImuApiService::handleCalibrateOrientation(PsychicRequest* request) {
    if (!_runtime) {
        return Response::error(request, 503, kUnavailableError);
    }

    const IMU::OrientationCalibrationResult result = _runtime->calibrateOrientation();
    return Response::success(request, [&](JsonVariant& root) {
        root["ok"] = result.status == IMU::OrientationCalibrationStatus::Success;
        root[CONFIG::Keys::kStatus] = IMU::MATH::calibrationStatusToString(result.status);
        root["sample_count"] = result.sampleCount;
        root["accel_magnitude_mean"] = result.accelMagnitudeMean;
        root["accel_magnitude_variance"] = result.accelMagnitudeVariance;
        JsonObject baseline = root[CONFIG::Keys::kOrientationBaseline].to<JsonObject>();
        baseline[CONFIG::Keys::kX] = result.baseline.x;
        baseline[CONFIG::Keys::kY] = result.baseline.y;
        baseline[CONFIG::Keys::kZ] = result.baseline.z;
    });
}

esp_err_t ImuApiService::handleResetOrientationBaseline(PsychicRequest* request) {
    if (!_runtime) {
        return Response::error(request, 503, kUnavailableError);
    }

    if (!_runtime->resetOrientationBaseline()) {
        return Response::error(request, 500, "imu/reset_failed");
    }

    return Response::success(request, [&](JsonVariant& root) {
        root["ok"] = true;
        root[CONFIG::Keys::kStatus] = "reset";
    });
}

}  // namespace API
