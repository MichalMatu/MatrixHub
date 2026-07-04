#include "UsbTerminalWireCodec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_heap_caps.h>

#include "../../logs/utils/JsonFormatter.h"

namespace API {

namespace {

size_t measureEscapedJsonString(const char* s) {
    if (!s) {
        return 0;
    }

    size_t total = 0;
    while (*s != '\0') {
        switch (*s) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                total += 2;
                break;
            default:
                total += (static_cast<unsigned char>(*s) < 0x20) ? 6 : 1;
                break;
        }
        ++s;
    }

    return total;
}

} // namespace

size_t UsbTerminalWireCodec::measureTextJson(
    const char* prefix,
    const char* text,
    const char* suffix) {
    return strlen(prefix ? prefix : "") +
           measureEscapedJsonString(text ? text : "") +
           strlen(suffix ? suffix : "");
}

size_t UsbTerminalWireCodec::writeTextJson(
    const char* prefix,
    const char* text,
    const char* suffix,
    char* outJson,
    size_t capacity) {
    if (!outJson || capacity == 0) {
        return 0;
    }

    const char* safePrefix = prefix ? prefix : "";
    const char* safeText = text ? text : "";
    const char* safeSuffix = suffix ? suffix : "";
    const size_t required = measureTextJson(safePrefix, safeText, safeSuffix);
    if (required > capacity) {
        return 0;
    }

    const size_t prefixLen = strlen(safePrefix);
    memcpy(outJson, safePrefix, prefixLen);

    static constexpr char kHex[] = "0123456789ABCDEF";
    size_t escapedLen = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(safeText); *p != '\0'; ++p) {
        const unsigned char c = *p;
        switch (c) {
            case '"':
            case '\\':
                outJson[prefixLen + escapedLen++] = '\\';
                outJson[prefixLen + escapedLen++] = static_cast<char>(c);
                break;
            case '\b':
                outJson[prefixLen + escapedLen++] = '\\';
                outJson[prefixLen + escapedLen++] = 'b';
                break;
            case '\f':
                outJson[prefixLen + escapedLen++] = '\\';
                outJson[prefixLen + escapedLen++] = 'f';
                break;
            case '\n':
                outJson[prefixLen + escapedLen++] = '\\';
                outJson[prefixLen + escapedLen++] = 'n';
                break;
            case '\r':
                outJson[prefixLen + escapedLen++] = '\\';
                outJson[prefixLen + escapedLen++] = 'r';
                break;
            case '\t':
                outJson[prefixLen + escapedLen++] = '\\';
                outJson[prefixLen + escapedLen++] = 't';
                break;
            default:
                if (c < 0x20) {
                    outJson[prefixLen + escapedLen++] = '\\';
                    outJson[prefixLen + escapedLen++] = 'u';
                    outJson[prefixLen + escapedLen++] = '0';
                    outJson[prefixLen + escapedLen++] = '0';
                    outJson[prefixLen + escapedLen++] = kHex[(c >> 4) & 0x0F];
                    outJson[prefixLen + escapedLen++] = kHex[c & 0x0F];
                } else {
                    outJson[prefixLen + escapedLen++] = static_cast<char>(c);
                }
                break;
        }
    }

    const size_t suffixLen = strlen(safeSuffix);
    memcpy(outJson + prefixLen + escapedLen, safeSuffix, suffixLen);

    return prefixLen + escapedLen + suffixLen;
}

bool UsbTerminalWireCodec::buildTextJson(
    const char* prefix,
    const char* text,
    const char* suffix,
    char*& outJson,
    size_t& outLen) {
    outJson = nullptr;
    outLen = 0;
    outLen = measureTextJson(prefix, text, suffix);
    outJson = static_cast<char*>(heap_caps_malloc(outLen + 1, MALLOC_CAP_SPIRAM));
    if (!outJson) {
        return false;
    }

    const size_t written = writeTextJson(prefix, text, suffix, outJson, outLen);
    if (written != outLen) {
        heap_caps_free(outJson);
        outJson = nullptr;
        outLen = 0;
        return false;
    }
    outJson[outLen] = '\0';
    return true;
}

void UsbTerminalWireCodec::formatClientId(int fd, char* out, size_t len) {
    if (!out || len == 0) {
        return;
    }
    snprintf(out, len, "%d", fd);
}

int UsbTerminalWireCodec::parseClientId(const char* targetId) {
    if (!targetId || !targetId[0]) {
        return -1;
    }
    return atoi(targetId);
}

void UsbTerminalWireCodec::buildSessionJson(
    const USB_TERMINAL::SessionSnapshot& snapshot,
    char* out,
    size_t len) {
    if (!out || len == 0) {
        return;
    }

    const char* transportStr = USB_TERMINAL::toTransportString(snapshot.transport);
    snprintf(
        out,
        len,
        "{\"type\":\"session\",\"busy\":%s,\"owner\":%s,\"transport\":%s,\"connected\":true}",
        snapshot.busy ? "true" : "false",
        snapshot.owner ? "true" : "false",
        transportStr ? (strcmp(transportStr, "ws") == 0 ? "\"ws\"" : "\"telegram\"")
                     : "null");
}

}  // namespace API
