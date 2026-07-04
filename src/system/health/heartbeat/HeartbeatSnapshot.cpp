#include "HeartbeatSnapshot.h"

#include "../../../config/System.h"
#include "HeartbeatConfigStore.h"

#include <cstring>

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {

bool loadHeartbeatSnapshot(HeartbeatSnapshot& snapshot) {
    snapshot = {};
    snapshot.intervalMs = HEARTBEAT::DEFAULT_INTERVAL_MS;

    bool loaded = false;
    SYSTEM::HEARTBEAT_CONFIG::withConfig([&](const RTC::HeartbeatData& heartbeat) {
        loaded = true;

        uint32_t intervalMs = heartbeat.intervalMs;
        if (intervalMs < LIMITS::HEARTBEAT::MIN_INTERVAL_MS ||
            intervalMs > LIMITS::HEARTBEAT::MAX_INTERVAL_MS) {
            intervalMs = HEARTBEAT::DEFAULT_INTERVAL_MS;
        }
        snapshot.intervalMs = intervalMs;

        for (uint8_t i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
            RTC::HeartbeatSlot slot{};
            memcpy(&slot, &heartbeat.slots[i], sizeof(slot));
            if (!slot.isValid() || snapshot.activeCount >= RTC::kMaxHeartbeatSlots) {
                continue;
            }

            auto& activeSlot = snapshot.activeSlots[snapshot.activeCount++];
            activeSlot.index = i;
            activeSlot.slot = slot;
        }
    });

    return loaded;
}

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
