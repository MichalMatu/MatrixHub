#include "JsonResponseWriter.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>

namespace Utils {

namespace {

bool shouldDisableCachingForPath(const char* uri) {
    return uri && (strncmp(uri, "/api/", 5) == 0 || strncmp(uri, "/rest/", 6) == 0);
}

}  // namespace

bool JsonResponseWriter::begin() {
    return true;
}

JsonResponseWriter::JsonResponseWriter(httpd_req_t* req) : _req(req) {}

bool JsonResponseWriter::beginResponse() {
    if (httpd_resp_set_type(_req, "application/json") != ESP_OK) {
        return false;
    }

    if (shouldDisableCachingForPath(_req ? _req->uri : nullptr) &&
        httpd_resp_set_hdr(_req, "Cache-Control", "no-store") != ESP_OK) {
        return false;
    }

    return true;
}

bool JsonResponseWriter::raw(const char* str) {
    return raw(str, strlen(str));
}

bool JsonResponseWriter::raw(const char* data, size_t len) {
    if (!data || len == 0) return true;

    if (len > sizeof(_stagingBuf)) {
        if (!flushPending()) return false;
        return httpd_resp_send_chunk(_req, data, (ssize_t)len) == ESP_OK;
    }

    if (_stagingLen + len > sizeof(_stagingBuf) && !flushPending()) {
        return false;
    }

    memcpy(_stagingBuf + _stagingLen, data, len);
    _stagingLen += len;
    return true;
}

bool JsonResponseWriter::finish() {
    if (!flushPending()) {
        return false;
    }
    return httpd_resp_send_chunk(_req, NULL, 0) == ESP_OK;
}

bool JsonResponseWriter::flushPending() {
    if (_stagingLen == 0) {
        return true;
    }

    const bool ok = httpd_resp_send_chunk(_req, _stagingBuf, (ssize_t)_stagingLen) == ESP_OK;
    _stagingLen = 0;
    return ok;
}

bool JsonResponseWriter::fmt(const char* format, ...) {
    char b[128];
    const size_t bufSize = sizeof(b);
    
    va_list args;
    va_start(args, format);
    int n = vsnprintf(b, bufSize, format, args);
    va_end(args);
    
    if (n < 0) return false;
    if ((size_t)n >= bufSize) {
        n = (int)(bufSize - 1);
        b[n] = '\0';
    }
    return raw(b, (size_t)n);
}

bool JsonResponseWriter::string(const char* s) {
    if (!raw("\"")) return false;
    if (!sendEscaped(s ? s : "")) return false;
    return raw("\"");
}

bool JsonResponseWriter::key(const char* k) {
    if (!string(k)) return false;
    return raw(":");
}

bool JsonResponseWriter::value(bool v) {
    return raw(v ? "true" : "false");
}

bool JsonResponseWriter::value(int v) {
    return fmt("%d", v);
}

bool JsonResponseWriter::value(unsigned int v) {
    return fmt("%u", v);
}

bool JsonResponseWriter::value(long v) {
    return fmt("%ld", v);
}

bool JsonResponseWriter::value(unsigned long v) {
    return fmt("%lu", v);
}

bool JsonResponseWriter::value(long long v) {
    return fmt("%lld", v);
}

bool JsonResponseWriter::value(unsigned long long v) {
    return fmt("%llu", v);
}

bool JsonResponseWriter::value(float v, int decimals) {
    if (std::isnan(v)) return raw("null");
    char fmtStr[8];
    snprintf(fmtStr, sizeof(fmtStr), "%%.%df", decimals);
    return fmt(fmtStr, v);
}

bool JsonResponseWriter::value(double v, int decimals) {
    if (std::isnan(v)) return raw("null");
    char fmtStr[8];
    snprintf(fmtStr, sizeof(fmtStr), "%%.%df", decimals);
    return fmt(fmtStr, v);
}

bool JsonResponseWriter::sendEscaped(const char* s) {
    char b[256];
    const size_t bufSize = sizeof(b);
    size_t pos = 0;
    
    auto flush = [&]() -> bool {
        if (pos == 0) return true;
        bool ok = raw(b, pos);
        pos = 0;
        return ok;
    };

    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        const unsigned char c = *p;
        if (c == '"' || c == '\\') {
            if (pos + 2 > bufSize && !flush()) return false;
            b[pos++] = '\\';
            b[pos++] = (char)c;
        } else if (c <= 0x1F) {
            // \u00XX
            if (pos + 6 > bufSize && !flush()) return false;
            static const char hex[] PROGMEM = "0123456789abcdef";
            b[pos++] = '\\';
            b[pos++] = 'u';
            b[pos++] = '0';
            b[pos++] = '0';
            b[pos++] = hex[(c >> 4) & 0x0F];
            b[pos++] = hex[c & 0x0F];
        } else {
            // FIXED Buffer Overflow Logic:
            // Ensure there is room for at least 1 character to be written,
            // otherwise flush first.
            if (pos >= bufSize && !flush()) return false;
            b[pos++] = (char)c;
        }
    }
    return flush();
}

} // namespace Utils
