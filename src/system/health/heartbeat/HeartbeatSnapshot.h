#pragma once

#include "../../rtc/types/RtcSystemTypes.h"

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {

struct HeartbeatActiveSlot {
    uint8_t index = 0;
    RTC::HeartbeatSlot slot{};
};

struct HeartbeatSnapshot {
    uint32_t intervalMs = 0;
    uint8_t activeCount = 0;
    HeartbeatActiveSlot activeSlots[RTC::kMaxHeartbeatSlots]{};

    bool hasActiveSlots() const {
        return activeCount > 0;
    }
};

bool loadHeartbeatSnapshot(HeartbeatSnapshot& snapshot);

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
