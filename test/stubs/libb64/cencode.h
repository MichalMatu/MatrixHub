#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int unused;
} base64_encodestate;

static inline void base64_init_encodestate(base64_encodestate* state) {
    (void)state;
}

static inline size_t base64_encode_expected_len(size_t len) {
    return ((len + 2u) / 3u) * 4u;
}

static inline int base64_encode_block(const char* input,
                                      int input_length,
                                      char* output,
                                      base64_encodestate* state) {
    (void)state;
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int out = 0;
    for (int i = 0; i < input_length; i += 3) {
        const unsigned int b0 = static_cast<unsigned char>(input[i]);
        const unsigned int b1 = (i + 1 < input_length)
            ? static_cast<unsigned char>(input[i + 1])
            : 0u;
        const unsigned int b2 = (i + 2 < input_length)
            ? static_cast<unsigned char>(input[i + 2])
            : 0u;

        output[out++] = kAlphabet[(b0 >> 2) & 0x3F];
        output[out++] = kAlphabet[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)];
        output[out++] = (i + 1 < input_length)
            ? kAlphabet[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)]
            : '=';
        output[out++] = (i + 2 < input_length)
            ? kAlphabet[b2 & 0x3F]
            : '=';
    }

    return out;
}

static inline int base64_encode_blockend(char* output, base64_encodestate* state) {
    (void)output;
    (void)state;
    return 0;
}

#ifdef __cplusplus
}
#endif
