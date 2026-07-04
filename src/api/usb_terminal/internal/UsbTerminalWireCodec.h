#pragma once

#include <cstddef>

#include <esp_err.h>

#include "../../../usb_terminal/UsbTerminalTypes.h"

namespace API {

class UsbTerminalWireCodec {
public:
    static size_t measureTextJson(
        const char* prefix,
        const char* text,
        const char* suffix);

    static size_t writeTextJson(
        const char* prefix,
        const char* text,
        const char* suffix,
        char* outJson,
        size_t capacity);

    static bool buildTextJson(
        const char* prefix,
        const char* text,
        const char* suffix,
        char*& outJson,
        size_t& outLen);

    static void formatClientId(int fd, char* out, size_t len);
    static int parseClientId(const char* targetId);
    static void buildSessionJson(
        const USB_TERMINAL::SessionSnapshot& snapshot,
        char* out,
        size_t len);
};

}  // namespace API
