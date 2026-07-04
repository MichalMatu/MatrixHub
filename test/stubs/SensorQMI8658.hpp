#pragma once

#include <stdint.h>

class SensorQMI8658 {
public:
    enum AccelRange {
        ACC_RANGE_2G,
        ACC_RANGE_4G,
        ACC_RANGE_8G,
        ACC_RANGE_16G
    };

    enum GyroRange {
        GYR_RANGE_16DPS,
        GYR_RANGE_32DPS,
        GYR_RANGE_64DPS,
        GYR_RANGE_128DPS,
        GYR_RANGE_256DPS,
        GYR_RANGE_512DPS,
        GYR_RANGE_1024DPS,
    };

    enum AccelODR {
        ACC_ODR_1000Hz = 3,
        ACC_ODR_500Hz,
        ACC_ODR_250Hz,
        ACC_ODR_125Hz,
        ACC_ODR_62_5Hz,
        ACC_ODR_31_25Hz,
    };

    enum GyroODR {
        GYR_ODR_7174_4Hz,
        GYR_ODR_3587_2Hz,
        GYR_ODR_1793_6Hz,
        GYR_ODR_896_8Hz,
        GYR_ODR_448_4Hz,
        GYR_ODR_224_2Hz,
        GYR_ODR_112_1Hz,
        GYR_ODR_56_05Hz,
        GYR_ODR_28_025Hz
    };

    enum LpfMode {
        LPF_MODE_0,
        LPF_MODE_1,
        LPF_MODE_2,
        LPF_MODE_3,
        LPF_OFF,
    };

    template <typename TWire>
    bool begin(TWire& wire, uint8_t addr = 0x6B, int sda = -1, int scl = -1) {
        (void)wire;
        (void)addr;
        (void)sda;
        (void)scl;
        return true;
    }

    uint8_t getChipID() const { return 0x6B; }
    void configAccelerometer(AccelRange range, AccelODR odr, LpfMode mode) {
        (void)range;
        (void)odr;
        (void)mode;
    }
    void enableAccelerometer() {}
    void configGyroscope(GyroRange range, GyroODR odr, LpfMode mode) {
        (void)range;
        (void)odr;
        (void)mode;
    }
    void enableGyroscope() {}
    float getAccelerometerScales() const { return 1.0f / 8192.0f; }
    float getGyroscopeScales() const { return 1.0f / 16.0f; }

    bool getAccelerometer(float& x, float& y, float& z) {
        x = 0.0f;
        y = 0.0f;
        z = 1.0f;
        return true;
    }
};
