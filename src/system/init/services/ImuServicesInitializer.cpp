#include "ImuServicesInitializer.h"

#include "../../../sensors/imu/ImuService.h"
#include "../../../sensors/imu/ImuManager.h"
#include "../../../sensors/imu/ImuRuntimeService.h"
#include "../../../alarms/AlarmService.h"
#include "../../logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "ImuInit"

void ImuServicesInitializer::initialize(State state) {
    state.imuService = std::make_unique<ImuService>();
    state.imuManager = std::make_unique<IMU::ImuManager>(state.imuService.get());
    state.imuRuntimeService = std::make_unique<IMU::ImuRuntimeService>(
        state.fs,
        state.imuService.get(),
        state.imuManager.get(),
        state.alarmService);
    state.imuRuntimeService->begin();

    LOGI("IMU services initialized (idle runtime)");
}
