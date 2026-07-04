#pragma once

#include <Arduino.h>

namespace NOTIFICATIONS {
namespace TELEGRAM {

class TelegramConnectionValidator {
public:
    // Returns error as const char** to avoid String allocation
    static bool ensureOnline(const char** errorOut, uint32_t ntpWaitMs);
    static bool resolveApiHost(IPAddress& outAddress);
    static void invalidateReachabilityCache(bool clearDns = true);

private:
    static bool isSystemTimeValid();
    static bool isYearValid(int year);
};

}  // namespace TELEGRAM
}  // namespace NOTIFICATIONS
