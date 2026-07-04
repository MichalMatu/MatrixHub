/**
 * @file AlarmInputData.h
 * @brief DTO for alarm evaluation inputs
 */

#pragma once

#include <cmath>

namespace ALARMS {

/**
 * Sensor input data for alarm evaluation.
 * All fields default to NAN so callers can omit values.
 */
struct AlarmInputData {
    float co2 = NAN;
    float temperature = NAN;
    float humidity = NAN;
    float wifiVariance = NAN;
    float wifiCsiMotion = NAN;
    float imuTamper = NAN;
    float gpioDigital = NAN;
    // Optional BLE fields for rule evaluation
    float bleTemp = NAN;
    float bleHumid = NAN;
    float bleBattery = NAN;
    float bleRssi = NAN;
};

}  // namespace ALARMS
