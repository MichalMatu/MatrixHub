/**
 * @file WifiSensingTaskRunner.h
 * @brief FreeRTOS task runner extracted from WifiSensingService
 * 
 * Encapsulates the sampling task loop logic: sampling, stats, motion detection,
 * alarm evaluation, and stack monitoring.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../sampling/RssiSampler.h"
#include "MotionDetector.h"
#include "../analysis/RssiVarianceAnalyzer.h" // Defines RssiStats
#include "../../config/System.h"
#include "../../system/utils/ScopeLock.h"
#include <atomic>
#include <vector>
#include <freertos/semphr.h>

namespace ALARMS { class AlarmService; }

namespace WIFISENSING {

// Task stack size (bytes; ESP-IDF Xtensa uses StackType_t = uint8_t)
static constexpr uint32_t SENSING_TASK_STACK_SIZE = CONFIG::TASKS::STACK_WIFI_SENSING_RSSI;

/**
 * @brief Manages FreeRTOS task lifecycle and loop logic.
 */
class WifiSensingTaskRunner {
 public:
  WifiSensingTaskRunner(RssiSampler& sampler, float varianceThreshold);
  ~WifiSensingTaskRunner();

  /**
   * @brief Start the sampling task.
   * @param sampleIntervalMs Delay between samples.
   * @return true if task created successfully.
   */
  bool start(uint32_t sampleIntervalMs);

  /**
   * @brief Stop the sampling task gracefully.
   * @return true only when the worker fully reached the suspended/deletable
   * state and its task resources are no longer live.
   */
  bool stop();

  /**
   * @brief Check if task is running.
   */
  bool isRunning() const { return _running.load(std::memory_order_acquire); }

  /**
   * @brief Get current motion detection state (from MotionDetector with hysteresis).
   */
  bool isMotionDetected() const { return _motionDetector.isMotionDetected(); }

  /**
   * @brief Update variance threshold at runtime.
   */
  void setVarianceThreshold(float threshold) { _varianceThreshold = threshold; }
  void setAlarmService(ALARMS::AlarmService* alarmService) { _alarmService = alarmService; }

  // Multicast callback support - allows multiple listeners
  using SensingCallback = std::function<void(const RssiSample&, const RssiStats&, bool isMotion)>;
  
  static constexpr size_t MAX_CALLBACKS = 4;
  void addCallback(SensingCallback cb) { 
      SYSTEM::ScopeLock lock(_mutex);
      if (lock.isLocked() && cb && _callbackCount < MAX_CALLBACKS) {
          _callbacks[_callbackCount++] = cb;
      } 
  }
  
  void clearCallbacks() { 
      SYSTEM::ScopeLock lock(_mutex);
      if (lock.isLocked()) {
          for (size_t i = 0; i < _callbackCount; i++) {
              _callbacks[i] = nullptr; 
          }
          _callbackCount = 0;
      } 
  }
  
  /*
  // Legacy single callback (replaces all)
  void setCallback(SensingCallback cb) { 
      SYSTEM::ScopeLock lock(_mutex);
      if (lock.isLocked()) {
          _callbacks.clear(); 
          if (cb) _callbacks.push_back(cb); 
      }
  }
  */

 private:
  static void taskEntry(void* param);
  void taskLoop();
  bool reapStoppedTask(TickType_t waitTicks);
  bool allocateTaskResources();
  void freeTaskResources();

  RssiSampler& _sampler;
  float _varianceThreshold;
  uint32_t _sampleIntervalMs = 1000;
  
  TaskHandle_t _taskHandle = nullptr;
  StackType_t* _taskStack = nullptr;
  StaticTask_t* _taskTcb = nullptr;
  std::atomic<bool> _running{false};
  
  MotionDetector _motionDetector;
  uint32_t _lastAlarmEvalMs = 0;
  
  SensingCallback _callbacks[MAX_CALLBACKS];
  size_t _callbackCount = 0;
  SemaphoreHandle_t _mutex;
  SemaphoreHandle_t _stopAck = nullptr;
  ALARMS::AlarmService* _alarmService = nullptr;
};

}  // namespace WIFISENSING
