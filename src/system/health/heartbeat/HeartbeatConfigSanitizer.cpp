#include "HeartbeatConfigSanitizer.h"

#include "../../../config/System.h"
#include "../../logging/Logging.h"

#include <cstdio>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "HeartCfg"

namespace SYSTEM {
namespace HEARTBEAT_DETAIL {
namespace {

bool isSafeString(const char* buf, size_t maxLen, bool allowExtendedUtf8, size_t* outLen) {
    if (!buf || maxLen == 0) {
        return false;
    }

    size_t len = 0;
    for (size_t i = 0; i < maxLen; i++) {
        const unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c == '\0') {
            if (outLen) {
                *outLen = len;
            }
            return true;
        }
        if (c < 32 || c == 127 || (!allowExtendedUtf8 && c > 126)) {
            if (outLen) {
                *outLen = len;
            }
            return false;
        }
        len++;
    }

    if (outLen) {
        *outLen = maxLen;
    }
    return false;
}

bool hasAnyScheme(const char* url) {
    return url && strstr(url, "://") != nullptr;
}

} // namespace

bool isSafeHeartbeatLabel(const char* buf, size_t maxLen, size_t* outLen) {
    return isSafeString(buf, maxLen, true, outLen);
}

bool isSafeHeartbeatUrl(const char* buf, size_t maxLen, size_t* outLen) {
    return isSafeString(buf, maxLen, false, outLen);
}

bool startsWithHttp(const char* url) {
    return url && strncmp(url, "http://", 7) == 0;
}

bool startsWithHttps(const char* url) {
    return url && strncmp(url, "https://", 8) == 0;
}

bool hasKnownHttpScheme(const char* url) {
    return startsWithHttp(url) || startsWithHttps(url);
}

HeartbeatSanitizeResult sanitizeConfig(RTC::HeartbeatData& heartbeat) {
    HeartbeatSanitizeResult result{};

    if (heartbeat.intervalMs < LIMITS::HEARTBEAT::MIN_INTERVAL_MS ||
        heartbeat.intervalMs > LIMITS::HEARTBEAT::MAX_INTERVAL_MS) {
        LOGW("Heartbeat interval out of range (%lu ms), restoring default %lu ms",
             static_cast<unsigned long>(heartbeat.intervalMs),
             static_cast<unsigned long>(HEARTBEAT::DEFAULT_INTERVAL_MS));
        heartbeat.intervalMs = HEARTBEAT::DEFAULT_INTERVAL_MS;
        result.changed = true;
        result.intervalCorrected = true;
    }

    for (uint8_t i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
        auto& slot = heartbeat.slots[i];

        if (!slot.enabled) {
            if (slot.allowInsecure) {
                slot.allowInsecure = false;
                result.changed = true;
            }
            continue;
        }

        if (!isSafeHeartbeatLabel(slot.name, sizeof(slot.name), nullptr)) {
            LOGW("Slot %u name is invalid, clearing", i);
            slot.name[0] = '\0';
            result.changed = true;
        }

        size_t urlLen = 0;
        if (!isSafeHeartbeatUrl(slot.url, sizeof(slot.url), &urlLen) || urlLen == 0) {
            LOGW("Slot %u URL is invalid or empty, disabling slot", i);
            slot.enabled = false;
            slot.allowInsecure = false;
            slot.url[0] = '\0';
            slot.name[0] = '\0';
            result.changed = true;
            continue;
        }

        if (!hasKnownHttpScheme(slot.url)) {
            if (!hasAnyScheme(slot.url)) {
                char normalized[sizeof(slot.url)];
                const int written = snprintf(normalized, sizeof(normalized), "http://%s", slot.url);
                if (written <= 0 || static_cast<size_t>(written) >= sizeof(normalized)) {
                    LOGW("Slot %u URL is too long after scheme normalization, disabling slot", i);
                    slot.enabled = false;
                    slot.allowInsecure = false;
                    slot.url[0] = '\0';
                    slot.name[0] = '\0';
                } else {
                    strlcpy(slot.url, normalized, sizeof(slot.url));
                    slot.allowInsecure = false;
                    char safeUrl[96];
                    LOG::Logging::redactUrlForLog(slot.url, safeUrl, sizeof(safeUrl));
                    LOGI("Slot %u URL normalized to '%s'", i, safeUrl);
                }
                result.changed = true;
            } else {
                LOGW("Slot %u uses unsupported URL scheme, disabling slot", i);
                slot.enabled = false;
                slot.allowInsecure = false;
                slot.url[0] = '\0';
                slot.name[0] = '\0';
                result.changed = true;
                continue;
            }
        }

        if (!startsWithHttps(slot.url) && slot.allowInsecure) {
            slot.allowInsecure = false;
            result.changed = true;
        }

        if (slot.isValid()) {
            result.hasActiveSlots = true;
        }
    }

    return result;
}

} // namespace HEARTBEAT_DETAIL
} // namespace SYSTEM
