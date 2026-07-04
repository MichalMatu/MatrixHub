/**
 * @file MotionDetector.cpp
 * @brief Motion detection logic implementation
 */

#include "MotionDetector.h"
#include <cmath>

namespace WIFISENSING {

bool MotionDetector::shouldPush(float variance, float threshold, uint32_t now) {
  // Hysteresis logic to prevent state flapping
  // If already detected, use lower threshold to clear (0.6 * threshold)
  // If not detected, must exceed full threshold to trigger
  
  if (std::isnan(variance)) variance = 0.0f;

  bool motionNow = _motionDetected;
  
  if (_motionDetected) {
      // CLEAR condition: Variance drops significantly below threshold
      if (variance < (threshold * 0.6f)) {
          motionNow = false;
      }
  } else {
      // TRIGGER condition: Variance exceeds threshold
      if (variance >= threshold) {
          motionNow = true;
      }
  }

  bool stateChanged = (motionNow != _motionDetected);
  bool timeToPush = (now - _lastPushMs >= PUSH_INTERVAL_MS);

  if (stateChanged || (motionNow && timeToPush)) {
    _motionDetected = motionNow;
    _lastPushMs = now;
    return true;
  }

  return false;
}

void MotionDetector::reset() {
  _motionDetected = false;
  _lastPushMs = 0;
}

}  // namespace WIFISENSING
