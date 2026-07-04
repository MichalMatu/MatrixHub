#include "RuntimeTasksInitializer.h"

#include "../../../sensors/SensorLoggingTask.h"
#include "../../../system/datalogger/BinaryDataLogger.h"
#include "../../../udp/UdpPusher.h"
#include "../../health/heartbeat/Heartbeat.h"
#include "../../logging/LogOutput.h"
#include "../../logging/Logging.h"
#include "../../matrix/MatrixTask.h"
#include "../../rtc/RtcConfig.h"
#include "../../services/ServiceRegistry.h"
#include "../../usb/UsbBootPolicy.h"

#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "TaskInit"

namespace SYSTEM {
namespace {

void startUsbLogging() {
    LOGI("[Phase6] Starting USB stack...");
    LOG::Output::beginUsb();
    LOGI("[Phase6] USB stack started");
}

void startMatrixTask(ServiceRegistry& services) {
    MATRIX::MatrixTask::start(
        services.getMatrixMenu(),
        services.getImuService(),
        services.getImuManager(),
        services.getMatrixService(),
        services.getMatrixManager(),
        services.getBleService(),
        services.getWifiSensingService(),
        services.getCsiService());
}

void startSensorPipeline(ServiceRegistry& services) {
    SensorLoggingTask::setAlarmService(services.getAlarmService());
    SensorLoggingTask::begin();
    if (!SensorLoggingTask::isRunning()) {
        // begin() can fail on task creation/resource issues. Require a live task
        // before Phase 6 succeeds so runtime matches the boot contract from Phase 5.
        LOGE("[Phase6] Sensor logging task failed to start");
        std::abort();
    }
    SensorLoggingTask::sendCommand(CMD_FORCE_READ_AND_LOG);
}

void configureUdpPusher(ServiceRegistry& services) {
    if (auto* udpPusher = services.getUdpPusher()) {
        // Phase 6 owns the "runtime is now live" handoff for UDP pushing.
        // If boot order ever regresses, this is the one place to check whether
        // the registry-owned instance was initialized before main-loop updates.
        udpPusher->begin();
    }
}

}  // namespace

void RuntimeTasksInitializer::initialize(ServiceRegistry& services) {
    if (UsbBootPolicy::shouldStartUsbOnBoot()) {
        startUsbLogging();
    } else {
        LOGI("[Phase6] Skipping USB stack start - no persisted USB feature is enabled");
    }
    startMatrixTask(services);
    DATALOG::BinaryDataLogger::begin();
    Heartbeat::begin();
    startSensorPipeline(services);
    configureUdpPusher(services);

    LOGI("[Phase6] Tasks initialized");
}

}  // namespace SYSTEM
