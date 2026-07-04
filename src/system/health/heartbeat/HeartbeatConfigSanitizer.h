#pragma once

#include "../../rtc/types/RtcSystemTypes.h"

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {

struct HeartbeatSanitizeResult {
    bool changed = false;
    bool hasActiveSlots = false;
    bool intervalCorrected = false;
};

bool isSafeHeartbeatLabel(const char* buf, size_t maxLen, size_t* outLen = nullptr);
bool isSafeHeartbeatUrl(const char* buf, size_t maxLen, size_t* outLen = nullptr);
bool startsWithHttp(const char* url);
bool startsWithHttps(const char* url);
bool hasKnownHttpScheme(const char* url);
HeartbeatSanitizeResult sanitizeConfig(RTC::HeartbeatData& heartbeat);

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
