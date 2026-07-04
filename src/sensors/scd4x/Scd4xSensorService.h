#pragma once

#include "../model/ISensorService.h"
#include <SensirionI2cScd4x.h>

namespace COMPENSATION {
class CompensationService;
}

namespace SENSORS {

/**
 * @brief SCD4x sensor service implementation.
 * Reads CO2, temperature and humidity from Sensirion SCD4x via I2C.
 * Implements ISensorService for dependency injection into SensorTaskLoop.
 */
class Scd4xSensorService : public ISensorService {
public:
    explicit Scd4xSensorService(COMPENSATION::CompensationService* compensationService = nullptr);
    ~Scd4xSensorService() override = default;

    bool begin() override;
    bool reinitialize() override;
    void readAll(SensorSnapshot& outSnap, PhaseStatus& outStatus) override;
    void sleep() override;
    void wake() override;

private:
    SensirionI2cScd4x _scd4x;
    bool _initialized = false;
    bool _sensorPresent = false;
    bool _selfHealingReinit = false;
    COMPENSATION::CompensationService* _compensationService;
};

} // namespace SENSORS
