#pragma once

#include "SensorTypes.h"

namespace SENSORS {

/**
 * @brief Abstract interface for sensor implementations.
 * Allows decoupling the sensor logic (SCD4x, Mock, etc.) from the task loop.
 */
class ISensorService {
public:
    virtual ~ISensorService() = default;

    /**
     * @brief Initialize the sensor hardware.
     * @return true if successful, false otherwise.
     */
    virtual bool begin() = 0;

    /**
     * @brief Perform a full readout cycle.
     * @param outSnap Snapshot to populate with new data.
     * @param outStatus Status object to populate with timing/error info.
     */
    virtual void readAll(SensorSnapshot& outSnap, PhaseStatus& outStatus) = 0;

    /**
     * @brief Force full re-initialization (for self-healing after I2C errors).
     * Resets internal state and re-runs the begin() sequence.
     * @return true if successful, false otherwise.
     */
    virtual bool reinitialize() = 0;

    /**
     * @brief Put sensor to low-power sleep mode.
     */
    virtual void sleep() = 0;

    /**
     * @brief Wake sensor from sleep.
     */
    virtual void wake() = 0;
};

} // namespace SENSORS
