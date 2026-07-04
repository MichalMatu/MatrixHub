/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   MIT License:
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 **/

#include <network/NTPStatus.h>
#include <time.h>
#include <lwip/apps/sntp.h>

namespace {
constexpr int kMinValidYear = 2025;
constexpr int kMaxValidYear = 2040;

// Appends JSON-escaped string content (no surrounding quotes).
// Escapes: \\  \"  and control chars as \u00XX.
static bool appendJsonEscaped(char *out, size_t outSize, size_t &pos, const char *s) {
    if (!s) s = "";
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        const unsigned char c = *p;
        if (c == '"' || c == '\\') {
            if (pos + 2 >= outSize) return false;
            out[pos++] = '\\';
            out[pos++] = (char)c;
            continue;
        }
        if (c < 0x20) {
            if (pos + 6 >= outSize) return false;
            // \u00XX
            static const char kHex[] = "0123456789ABCDEF";
            out[pos++] = '\\';
            out[pos++] = 'u';
            out[pos++] = '0';
            out[pos++] = '0';
            out[pos++] = kHex[(c >> 4) & 0x0F];
            out[pos++] = kHex[c & 0x0F];
            continue;
        }
        if (pos + 1 >= outSize) return false;
        out[pos++] = (char)c;
    }
    return true;
}

static bool formatUtcIso8601(char *out, size_t outSize, time_t now) {
    struct tm tmUtc;
    gmtime_r(&now, &tmUtc);
    return strftime(out, outSize, "%FT%TZ", &tmUtc) > 0;
}

static bool formatLocalIso8601WithOffset(char *out, size_t outSize, time_t now) {
    struct tm tmLocal;
    localtime_r(&now, &tmLocal);

    char datePart[32];
    if (strftime(datePart, sizeof(datePart), "%FT%T", &tmLocal) == 0) return false;

    char offset[8]; // +hhmm
    if (strftime(offset, sizeof(offset), "%z", &tmLocal) == 0) return false;

    // Expect +hhmm (len 5). If not, fallback to datePart only.
    if (strlen(offset) == 5) {
        // +hhmm -> +hh:mm
        return snprintf(out, outSize, "%s%c%c%c:%c%c",
                       datePart, offset[0], offset[1], offset[2], offset[3], offset[4]) > 0;
    }
    return snprintf(out, outSize, "%s", datePart) > 0;
}

static bool isSystemTimeValid(time_t now) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    const int year = timeinfo.tm_year + 1900;
    return year >= kMinValidYear && year <= kMaxValidYear;
}

} // namespace

NTPStatus::NTPStatus(PsychicHttpServer *server, SecurityManager *securityManager) : _server(server),
                                                                                    _securityManager(securityManager)
{
}

void NTPStatus::begin()
{
    _server->on(NTP_STATUS_SERVICE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(
                    [this](PsychicRequest *request)
                    { return this->ntpStatus(request); },
                    AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", NTP_STATUS_SERVICE_PATH);
}

esp_err_t NTPStatus::ntpStatus(PsychicRequest *request)
{
    httpd_req_t *req = request->request();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    const time_t now = time(nullptr);
    const uint32_t uptime = millis() / 1000;
    const int status = sntp_enabled() ? 1 : 0;
    const bool timeValid = isSystemTimeValid(now);

    char utc[32] = {0};
    char local[40] = {0};
    const bool utcOk = formatUtcIso8601(utc, sizeof(utc), now);
    const bool localOk = formatLocalIso8601WithOffset(local, sizeof(local), now);

    if (!utcOk || !localOk) {
        // NTP status is used during login/bootstrap to decide whether we need
        // a best-effort manual time recovery. Returning HTTP 500 here turns a
        // recoverable "time not trustworthy yet" state into a transport error
        // and leaves the frontend blind. Degrade to empty timestamps instead.
        ESP_LOGW(SVK_TAG, "ntpStatus: time formatting unavailable (utc=%d local=%d)", utcOk ? 1 : 0, localOk ? 1 : 0);
    }

    const char *serverName = sntp_getservername(0);

    char buf[576];
    size_t pos = 0;

    auto append = [&](const char *fmt, ...) -> bool {
        if (pos >= sizeof(buf)) return false;
        va_list args;
        va_start(args, fmt);
        const int n = vsnprintf(buf + pos, sizeof(buf) - pos, fmt, args);
        va_end(args);
        if (n < 0) return false;
        if ((size_t)n >= sizeof(buf) - pos) return false;
        pos += (size_t)n;
        return true;
    };

    if (!append("{\"status\":%d,\"time_valid\":%s,\"utc_time\":\"%s\",\"local_time\":\"%s\",\"server\":\"",
                status,
                timeValid ? "true" : "false",
                utc,
                local)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ntpStatus: buffer overflow");
        return ESP_FAIL;
    }
    if (!appendJsonEscaped(buf, sizeof(buf), pos, serverName)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ntpStatus: buffer overflow");
        return ESP_FAIL;
    }
    if (!append("\",\"uptime\":%u}", (unsigned)uptime)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ntpStatus: buffer overflow");
        return ESP_FAIL;
    }

    httpd_resp_send(req, buf, (ssize_t)pos);
    return ESP_OK;
}
