#pragma once

#include "ImuTypes.h"

#include <cstdint>

namespace IMU {

class ImuAlarmDetector {
public:
    ImuAlarmStatus update(const ImuAlarmConfig& config,
                          const ImuMetrics& metrics,
                          uint32_t nowMs);
    void reset();

private:
    bool _triggered = false;
    bool _triggerHoldActive = false;
    bool _clearHoldActive = false;
    uint32_t _triggerHoldStartMs = 0;
    uint32_t _clearHoldStartMs = 0;
    ImuAlarmReason _activeReason = ImuAlarmReason::None;
};

}  // namespace IMU
