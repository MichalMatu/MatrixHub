#pragma once

#include <memory>

namespace COMPENSATION {
class CompensationService;
}

namespace BLE {
class BleService;
}

namespace ALARMS {
class AlarmService;
}

namespace SENSORS {
class Scd4xSensorService;
}

namespace WIFISENSING {
class WifiSensingService;
namespace CSI {
class CsiService;
}
}

namespace MATRIX_MANAGER {
class MatrixManagerService;
}

class CoreServicesInitializer {
public:
    static void initialize(
        std::unique_ptr<COMPENSATION::CompensationService>& compensationService,
        std::unique_ptr<BLE::BleService>& bleService,
        std::unique_ptr<ALARMS::AlarmService>& alarmService,
        std::unique_ptr<SENSORS::Scd4xSensorService>& sensorService,
        std::unique_ptr<WIFISENSING::CSI::CsiService>& csiService,
        std::unique_ptr<WIFISENSING::WifiSensingService>& wifiSensingService,
        MATRIX_MANAGER::MatrixManagerService* matrixManager);
};
