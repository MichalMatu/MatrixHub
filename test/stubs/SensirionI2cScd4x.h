#ifndef SENSIRIONI2CSCD4X_H
#define SENSIRIONI2CSCD4X_H

#include "Wire.h"

#include <cstddef>
#include <cstdint>

namespace TEST_STUBS {

struct SensirionI2cScd4xState {
    static constexpr size_t kMaxQueuedErrors = 8;

    int beginCalls = 0;
    int stopPeriodicCalls = 0;
    int reinitCalls = 0;
    int startPeriodicCalls = 0;

    int16_t stopPeriodicDefaultError = 0;
    int16_t reinitError = 0;
    int16_t serialError = 0;
    int16_t temperatureOffsetError = 0;
    int16_t startPeriodicError = 0;
    int16_t dataReadyError = 0;
    int16_t readMeasurementError = 0;

    bool dataReady = true;
    uint16_t co2 = 650;
    float temperature = 23.5f;
    float humidity = 48.0f;
    float temperatureOffset = 0.0f;
    uint64_t serialNumber = 0x123456789ABCDEF0ULL;

    int16_t stopPeriodicErrors[kMaxQueuedErrors]{};
    size_t stopPeriodicErrorCount = 0;
    size_t stopPeriodicErrorIndex = 0;
};

inline SensirionI2cScd4xState sensirionI2cScd4xState;

inline void resetSensirionI2cScd4xState() {
    sensirionI2cScd4xState = SensirionI2cScd4xState{};
}

inline void queueStopPeriodicMeasurementError(int16_t error) {
    auto& state = sensirionI2cScd4xState;
    if (state.stopPeriodicErrorCount < SensirionI2cScd4xState::kMaxQueuedErrors) {
        state.stopPeriodicErrors[state.stopPeriodicErrorCount++] = error;
    }
}

}  // namespace TEST_STUBS

class SensirionI2cScd4x {
public:
    void begin(TwoWire& i2cBus, uint8_t i2cAddress) {
        (void)i2cBus;
        (void)i2cAddress;
        ++TEST_STUBS::sensirionI2cScd4xState.beginCalls;
    }

    int16_t stopPeriodicMeasurement() {
        auto& state = TEST_STUBS::sensirionI2cScd4xState;
        ++state.stopPeriodicCalls;
        if (state.stopPeriodicErrorIndex < state.stopPeriodicErrorCount) {
            return state.stopPeriodicErrors[state.stopPeriodicErrorIndex++];
        }
        return state.stopPeriodicDefaultError;
    }

    int16_t reinit() {
        ++TEST_STUBS::sensirionI2cScd4xState.reinitCalls;
        return TEST_STUBS::sensirionI2cScd4xState.reinitError;
    }

    int16_t getSerialNumber(uint64_t& serialNumber) {
        serialNumber = TEST_STUBS::sensirionI2cScd4xState.serialNumber;
        return TEST_STUBS::sensirionI2cScd4xState.serialError;
    }

    int16_t getTemperatureOffset(float& temperatureOffset) {
        temperatureOffset = TEST_STUBS::sensirionI2cScd4xState.temperatureOffset;
        return TEST_STUBS::sensirionI2cScd4xState.temperatureOffsetError;
    }

    int16_t startPeriodicMeasurement() {
        ++TEST_STUBS::sensirionI2cScd4xState.startPeriodicCalls;
        return TEST_STUBS::sensirionI2cScd4xState.startPeriodicError;
    }

    int16_t getDataReadyStatus(bool& dataReady) {
        dataReady = TEST_STUBS::sensirionI2cScd4xState.dataReady;
        return TEST_STUBS::sensirionI2cScd4xState.dataReadyError;
    }

    int16_t readMeasurement(uint16_t& co2, float& temperature, float& humidity) {
        const auto& state = TEST_STUBS::sensirionI2cScd4xState;
        co2 = state.co2;
        temperature = state.temperature;
        humidity = state.humidity;
        return state.readMeasurementError;
    }

    int16_t powerDown() { return 0; }
    int16_t wakeUp() { return 0; }
};

#endif  // SENSIRIONI2CSCD4X_H
