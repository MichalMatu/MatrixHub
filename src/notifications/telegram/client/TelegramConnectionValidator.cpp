#include "TelegramConnectionValidator.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include "../../../config/App.h"
#include "../../../system/logging/Logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <lwip/netdb.h>

#undef LOG_TAG
#define LOG_TAG "TgConnVal"

namespace NOTIFICATIONS {
namespace TELEGRAM {

static const char* kTelegramHost = "api.telegram.org";

// Cache successful online checks to reduce DNS/TCP probe frequency.
// TelegramWorker polls every 10s, so probing every cycle is unnecessary.
namespace {
    constexpr uint32_t kOnlineCacheTtlMs = APP::NOTIFICATIONS::TELEGRAM_ONLINE_CACHE_TTL_MS;
    constexpr uint8_t kDnsAttempts = APP::NOTIFICATIONS::TELEGRAM_ONLINE_DNS_ATTEMPTS;
    constexpr uint8_t kTcpAttempts = APP::NOTIFICATIONS::TELEGRAM_ONLINE_TCP_ATTEMPTS;
    constexpr uint32_t kDnsRetryDelayMs = APP::NOTIFICATIONS::TELEGRAM_ONLINE_DNS_RETRY_DELAY_MS;
    constexpr uint32_t kTcpRetryDelayMs = APP::NOTIFICATIONS::TELEGRAM_ONLINE_TCP_RETRY_DELAY_MS;
    constexpr uint32_t kTimePollDelayMs = APP::NOTIFICATIONS::TELEGRAM_ONLINE_TIME_POLL_DELAY_MS;
    constexpr int kMinValidYear = APP::NOTIFICATIONS::TELEGRAM_VALID_YEAR_MIN;
    constexpr int kMaxValidYear = APP::NOTIFICATIONS::TELEGRAM_VALID_YEAR_MAX;
    constexpr uint16_t kTcpPort = APP::NOTIFICATIONS::TELEGRAM_ONLINE_TCP_PORT;
    uint32_t sLastOnlineOkMs = 0;
    uint32_t sLastDnsOkMs = 0;
    IPAddress sLastResolvedIp;

    void formatIpAddress(const IPAddress& address, char* out, size_t outSize) {
        if (!out || outSize == 0) {
            return;
        }

        snprintf(out,
                 outSize,
                 "%u.%u.%u.%u",
                 static_cast<unsigned>(address[0]),
                 static_cast<unsigned>(address[1]),
                 static_cast<unsigned>(address[2]),
                 static_cast<unsigned>(address[3]));
    }
}

bool TelegramConnectionValidator::isYearValid(int year) {
    return year >= kMinValidYear && year <= kMaxValidYear;
}

bool TelegramConnectionValidator::isSystemTimeValid() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return isYearValid(timeinfo.tm_year + 1900);
}

bool TelegramConnectionValidator::resolveApiHost(IPAddress& outAddress) {
    const uint32_t nowMs = millis();
    if (sLastDnsOkMs != 0 && (nowMs - sLastDnsOkMs) < kOnlineCacheTtlMs && sLastResolvedIp != IPAddress()) {
        outAddress = sLastResolvedIp;
        return true;
    }

    for (uint8_t attempt = 0; attempt < kDnsAttempts; attempt++) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(kTelegramHost, NULL, &hints, &res) == 0) {
            outAddress = IPAddress(((struct sockaddr_in*)(res->ai_addr))->sin_addr.s_addr);
            freeaddrinfo(res);
            if (outAddress != IPAddress()) {
                const IPAddress previousIp = sLastResolvedIp;
                sLastResolvedIp = outAddress;
                sLastDnsOkMs = millis();
                char ipBuf[16];
                formatIpAddress(outAddress, ipBuf, sizeof(ipBuf));
                if (previousIp == IPAddress()) {
                    LOGI("DNS resolved: host=%s ip=%s", kTelegramHost, ipBuf);
                } else if (previousIp != outAddress) {
                    char previousIpBuf[16];
                    formatIpAddress(previousIp, previousIpBuf, sizeof(previousIpBuf));
                    LOGI("DNS updated: host=%s ip=%s prev=%s", kTelegramHost, ipBuf, previousIpBuf);
                }
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kDnsRetryDelayMs));
    }

    return false;
}

void TelegramConnectionValidator::invalidateReachabilityCache(bool clearDns) {
    sLastOnlineOkMs = 0;
    if (clearDns) {
        sLastDnsOkMs = 0;
        sLastResolvedIp = IPAddress();
    }
}

bool TelegramConnectionValidator::ensureOnline(const char** errorOut, uint32_t ntpWaitMs) {
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_OFF) {
        *errorOut = "offline/wifi_off";
        return false;
    }

    if (!WiFi.isConnected()) {
        *errorOut = "offline/wifi_not_connected";
        return false;
    }

    // If we recently validated connectivity and system time is still valid,
    // skip the DNS+TCP probe to reduce lwIP churn and heap fragmentation risk.
    const uint32_t nowMs = millis();
    if (sLastOnlineOkMs != 0 && (nowMs - sLastOnlineOkMs) < kOnlineCacheTtlMs && isSystemTimeValid()) {
        return true;
    }

    // Hard policy: do not send notifications unless we can confirm internet reachability.
    // We use a fast DNS + TCP connect probe to api.telegram.org:443.
    // We use POSIX getaddrinfo directly instead of WiFi.hostByName because the latter 
    // triggers thread-unsafe DNS cache clearing in the Arduino ESP32 core.
    IPAddress resolved;
    if (!resolveApiHost(resolved)) {
        *errorOut = "offline/dns_failed";
        return false;
    }

    bool tcpOk = false;
    for (uint8_t attempt = 0; attempt < kTcpAttempts; attempt++) {
        WiFiClient probe;
        // WiFiClient::setTimeout is milliseconds in this core.
        probe.setTimeout(APP::NOTIFICATIONS::TELEGRAM_ONLINE_PROBE_TIMEOUT_MS);
        if (probe.connect(resolved, kTcpPort)) {
            tcpOk = true;
            probe.stop();
            break;
        }
        probe.stop();
        vTaskDelay(pdMS_TO_TICKS(kTcpRetryDelayMs));
    }
    if (!tcpOk) {
        invalidateReachabilityCache(true);
        *errorOut = "offline/tcp_connect_failed";
        return false;
    }

    // Also require a valid system time window (runtime policy).
    // We don't rely on SNTP sync-status APIs here because they may be unavailable.
    if (!isSystemTimeValid()) {
        const uint32_t start = millis();
        while ((millis() - start) < ntpWaitMs) {
            if (isSystemTimeValid()) {
                sLastOnlineOkMs = millis();
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(kTimePollDelayMs));
        }
        invalidateReachabilityCache(false);
        *errorOut = "offline/time_invalid";
        return false;
    }

    sLastOnlineOkMs = millis();
    return true;
}

}  // namespace TELEGRAM
}  // namespace NOTIFICATIONS
