#include "CoreServicesInitializer.h"

#include "../../../alarms/AlarmService.h"
#include "../../../ble/BleService.h"
#include "../../../compensation/CompensationService.h"
#include "../../../sensors/SensorLoggingTask.h"
#include "../../../sensors/scd4x/Scd4xSensorService.h"
#include "../../../wifisensing/WifiSensingService.h"
#include "../../../wifisensing/csi/core/CsiService.h"
#include "../../logging/Logging.h"
#include "../../rtc/RtcConfig.h"

#include <cstdlib>

void CoreServicesInitializer::initialize(
    std::unique_ptr<COMPENSATION::CompensationService>& compensationService,
    std::unique_ptr<BLE::BleService>& bleService,
    std::unique_ptr<ALARMS::AlarmService>& alarmService,
    std::unique_ptr<SENSORS::Scd4xSensorService>& sensorService,
    std::unique_ptr<WIFISENSING::CSI::CsiService>& csiService,
    std::unique_ptr<WIFISENSING::WifiSensingService>& wifiSensingService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager) {
    LOG::PhaseTimer coreTimer("Registry/Core");

    compensationService = std::make_unique<COMPENSATION::CompensationService>();
    compensationService->begin();
    LOG_PHASE_STEP(coreTimer, "CompensationService");

    bleService = std::make_unique<BLE::BleService>();
    LOG_PHASE_STEP(coreTimer, "BleService");

    alarmService = std::make_unique<ALARMS::AlarmService>(matrixManager, bleService.get());
    alarmService->begin();
    LOG_PHASE_STEP(coreTimer, "AlarmService");

    sensorService = std::make_unique<SENSORS::Scd4xSensorService>(compensationService.get());
    // Sensor logging feeds the normal telemetry/alarm path. If prepare() fails,
    // booting further would report the system as "up" while its main data path is dead.
    if (!SensorLoggingTask::prepare()) {
        LOGE("SensorLoggingTask init failed");
        std::abort();
    }
    SensorLoggingTask::setSensorService(sensorService.get());
    LOG_PHASE_STEP(coreTimer, "Scd4xSensorService");

    csiService = std::make_unique<WIFISENSING::CSI::CsiService>();
    csiService->begin();
    csiService->setConsumerActive(WIFISENSING::CSI::CsiConsumer::Boot, BOOT::CSI::START_ENABLED);
    LOG_PHASE_STEP(coreTimer, "CsiService");

    wifiSensingService = std::make_unique<WIFISENSING::WifiSensingService>(alarmService.get());
    LOG_PHASE_STEP(coreTimer, "WifiSensingService");

    LOG_PHASE_DONE(coreTimer, "Core services");
}
