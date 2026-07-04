#include "JsonFormatter.h"
#include <Arduino.h>

namespace API {
namespace Utils {

size_t fast_u32_to_str(char* out, uint32_t val) {
    char temp[10];
    char* p = temp;
    do {
        *p++ = (val % 10) + '0';
        val /= 10;
    } while (val > 0);
    
    size_t len = p - temp;
    while (p > temp) {
        *out++ = *--p;
    }
    return len;
}

size_t escapeJsonString(char* out, size_t maxLen, const char* s) {
    size_t pos = 0;
    if (!s) return 0;
    
    for (const unsigned char* p = (const unsigned char*)s; *p && pos < maxLen - 6; ++p) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            out[pos++] = '\\';
            out[pos++] = (char)c;
        } else if (c == '\n') {
            out[pos++] = '\\';
            out[pos++] = 'n';
        } else if (c == '\r') {
            out[pos++] = '\\';
            out[pos++] = 'r';
        } else if (c == '\t') {
            out[pos++] = '\\';
            out[pos++] = 't';
        } else if (c < 0x20) {
            // Control char -> \u00XX
            static const char kHex[] PROGMEM = "0123456789ABCDEF";
            out[pos++] = '\\';
            out[pos++] = 'u';
            out[pos++] = '0';
            out[pos++] = '0';
            out[pos++] = kHex[(c >> 4) & 0x0F];
            out[pos++] = kHex[c & 0x0F];
        } else {
            out[pos++] = (char)c;
        }
    }
    out[pos] = '\0';
    return pos;
}

} // namespace Utils
} // namespace API
