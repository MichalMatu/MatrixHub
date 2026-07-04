#pragma once

#include "ImuAlarmDetector.h"
#include "ImuManager.h"
#include "ImuTypes.h"
#include "../../system/rtc/RtcStatefulService.h"
#include "../../system/rtc/types/RtcImuTypes.h"

#include <FS.h>

class ImuService;
namespace ALARMS { class AlarmService; }

namespace IMU {

class ImuRuntimeService : public RtcStatefulService<RTC::ImuData> {
public:
    ImuRuntimeService(FS* fs,
                      ImuService* imuService,
                      ImuManager* imuManager,
                      ALARMS::AlarmService* alarmService = nullptr);

    void begin();
    void tick();

    ImuMetrics getMetrics() const;
    ImuAlarmStatus getAlarmStatus() const;
    ManagerStatus getManagerStatus() const;
    OrientationCalibrationResult calibrateOrientation();
    bool resetOrientationBaseline();
    void setAlarmService(ALARMS::AlarmService* alarmService);

    static void readState(RTC::ImuData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::ImuData& settings,
        std::string_view originId);

private:
    StateHandlerResult persistConfig();
    void applyConsumerState();
    bool updateMetricsFromCache(uint32_t nowMs);
    void updateAlarmDetector(uint32_t nowMs);
    void publishAlarmValue(const ImuAlarmStatus& status);
    static ImuAlarmConfig alarmConfigFromSettings(const RTC::ImuData& settings);
    void publishBaselineFromSettings(const RTC::ImuData& settings);

    FS* _fs = nullptr;
    ImuService* _imuService = nullptr;
    ImuManager* _imuManager = nullptr;
    ALARMS::AlarmService* _alarmService = nullptr;
    mutable portMUX_TYPE _metricsMux = portMUX_INITIALIZER_UNLOCKED;
    ImuMetrics _metrics;
    mutable portMUX_TYPE _alarmStatusMux = portMUX_INITIALIZER_UNLOCKED;
    ImuAlarmDetector _alarmDetector;
    ImuAlarmStatus _alarmStatus;
    uint32_t _lastMetricUpdateMs = 0;
};

}  // namespace IMU
