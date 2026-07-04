#pragma once

#include <Arduino.h>

#include <Wire.h>
#ifdef NATIVE_BUILD
#include "../../../test/stubs/SensorQMI8658.hpp"
#else
#include <SensorQMI8658.hpp>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include "ImuTypes.h"

/**
 * @class ImuService
 * @brief Controls the QMI8658 IMU on ESP32-S3-Matrix
 * 
 * Handles accelerometer/gyroscope reading and tap detection.
 */
class ImuService {
public:
    ImuService() = default;

    /**
     * @brief Initialize the IMU hardware (idempotent)
     * @return true if successful
     */
    bool begin();

    /**
     * @brief Stop IMU sampling and free resources
     */
    void stop();

    /**
     * @brief Main update loop
     */
    void loop();

    /**
     * @brief Check if a tap was detected recently
     * @return true if tapped
     */
    bool wasTapped();

    /**
     * @brief Check if IMU is initialized
     */
    bool isInitialized() const { return _initialized; }

    /**
     * @brief Get raw QMI8658 sensor instance (for AirMouseService)
     * @return Reference to the sensor
     */
    SensorQMI8658& getQmi() { return _qmi; }

    /**
     * @brief Thread-safe accelerometer read
     * @return true if successful
     */
    bool readAccelerometer(float& x, float& y, float& z);

    /**
     * @brief Non-blocking read of cached accelerometer values
     * @return true if values are valid (IMU initialized)
     */
    bool getCachedAccel(float& x, float& y, float& z) const;

    /**
     * @brief Non-blocking read of the latest cached Acc + Gyro sample
     * @return true if a fresh sample is available
     */
    bool getCachedSample(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) const;

    /**
     * @brief Non-blocking read of the latest cached sample including timestamp.
     * @return true if a fresh sample is available
     */
    bool getCachedSample(IMU::ImuSample& sample) const;

    /**
     * @brief Thread-safe read of all sensor data (Acc + Gyro)
     * @return true if successful
     */
    bool readAll(float& ax, float& ay, float& az, float& gx, float& gy, float& gz);

private:
   ImuService(const ImuService&) = delete;
   ImuService& operator=(const ImuService&) = delete;

   static void samplerTask(void* param);
   void runSamplerTask();
   bool readDirect(float& ax, float& ay, float& az, float& gx, float& gy, float& gz);
   void updateCachedSample(float ax, float ay, float az, float gx, float gy, float gz);
   void signalStopAck();
   bool reapStoppedTask(TickType_t waitTicks);
   void destroyTaskResources();

   SensorQMI8658 _qmi;
   std::atomic<bool> _initialized{false};
   std::atomic<bool> _stopRequested{false};
   volatile bool _tapDetected = false;
   SemaphoreHandle_t _mutex = nullptr;
   SemaphoreHandle_t _stopAck = nullptr;

   TaskHandle_t _samplerTaskHandle = nullptr;
   StackType_t* _samplerStack = nullptr;
   StaticTask_t* _samplerTcb = nullptr;

   mutable portMUX_TYPE _cacheMux = portMUX_INITIALIZER_UNLOCKED;
   float _cachedAx = 0.0f, _cachedAy = 0.0f, _cachedAz = 0.0f;
   float _cachedGx = 0.0f, _cachedGy = 0.0f, _cachedGz = 0.0f;
   uint32_t _sampleTimestampMs = 0;
   bool _sampleValid = false;
   float _accelScale = 0.0f;
   float _gyroScale = 0.0f;
};
