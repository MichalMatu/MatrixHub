#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int base64_decode_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2;
    return -1;
}

static inline size_t base64_decode_expected_len(size_t len) {
    return ((len + 3u) / 4u) * 3u;
}

static inline int base64_decode_chars(const char* input, size_t len, char* output) {
    int out = 0;

    for (size_t i = 0; i < len; ) {
        int values[4] = {-2, -2, -2, -2};
        size_t chunk = 0;
        for (; chunk < 4 && i < len; ++i) {
            const int value = base64_decode_value(input[i]);
            if (value < -1) {
                values[chunk++] = value;
            } else if (value >= 0) {
                values[chunk++] = value;
            }
        }
        while (chunk < 4) {
            values[chunk++] = -2;
        }

        if (values[0] < 0 || values[1] < 0) {
            break;
        }

        output[out++] = static_cast<char>((values[0] << 2) | (values[1] >> 4));

        if (values[2] >= 0) {
            output[out++] = static_cast<char>(((values[1] & 0x0F) << 4) | (values[2] >> 2));
        }

        if (values[3] >= 0) {
            output[out++] = static_cast<char>(((values[2] & 0x03) << 6) | values[3]);
        }
    }

    return out;
}

#ifdef __cplusplus
}
#endif
