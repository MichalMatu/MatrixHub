#pragma once

#include <Arduino.h>
#include <atomic>

#include "../../rtc/types/RtcSystemTypes.h"

namespace NETWORK {
class UnifiedHttpClient;
}

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {

class HeartbeatTransport {
public:
    explicit HeartbeatTransport(std::atomic<bool>* runningFlag);
    ~HeartbeatTransport() = default;

    bool ping(const RTC::HeartbeatSlot& slot, uint32_t timeoutMs, uint8_t retries);
    void cancelActiveIo();
    void release();
    void releaseIfIdle(uint32_t nowMs, uint32_t idleMs);

private:
    bool ensureClient();
    static void configureClientForSlot(NETWORK::UnifiedHttpClient& client, const RTC::HeartbeatSlot& slot);

    std::atomic<bool>* _runningFlag = nullptr;
    uint32_t _lastUseMs = 0;
    void* _clientStorage = nullptr;
    NETWORK::UnifiedHttpClient* _client = nullptr;
};

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
