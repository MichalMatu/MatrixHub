/**
 * @file MotionDetector.h
 * @brief Motion detection logic extracted from WifiSensingService task loop
 * 
 * Encapsulates motion state tracking and change detection.
 */

#pragma once

#include <cstdint>

namespace WIFISENSING {

/**
 * @brief Tracks motion state and detects changes for push notifications.
 */
class MotionDetector {
 public:
  MotionDetector() = default;

  /**
   * @brief Update motion state and check if push is needed.
   * @param variance Current RSSI variance.
   * @param threshold Variance threshold for motion detection.
   * @param now Current timestamp (millis).
   * @return true if state update should be published (rate-limited).
   */
  bool shouldPush(float variance, float threshold, uint32_t now);

  /**
   * @brief Get current motion detection state.
   */
  bool isMotionDetected() const { return _motionDetected; }

  /**
   * @brief Reset detector state.
   */
  void reset();

 private:
  bool _motionDetected = false;
  uint32_t _lastPushMs = 0;
  
  static constexpr uint32_t PUSH_INTERVAL_MS = 1000;  // 1s heartbeat
};

}  // namespace WIFISENSING
