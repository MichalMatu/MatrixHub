#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MBEDTLS_MD_NONE = 0,
    MBEDTLS_MD_SHA256 = 1
} mbedtls_md_type_t;

typedef struct {
    uint64_t state;
} mbedtls_md_context_t;

typedef struct {
    mbedtls_md_type_t type;
} mbedtls_md_info_t;

static inline uint64_t mbedtls_stub_mix(uint64_t seed, const unsigned char* data, size_t len) {
    const uint64_t kFnvPrime = 1099511628211ULL;
    uint64_t state = seed;
    for (size_t i = 0; i < len; ++i) {
        state ^= static_cast<uint64_t>(data[i]);
        state *= kFnvPrime;
    }
    return state;
}

static inline void mbedtls_md_init(mbedtls_md_context_t* ctx) {
    if (ctx) {
        ctx->state = 1469598103934665603ULL;
    }
}

static inline const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t type) {
    static mbedtls_md_info_t info;
    info.type = type;
    return &info;
}

static inline int mbedtls_md_setup(mbedtls_md_context_t* ctx, const mbedtls_md_info_t* info, int hmac) {
    (void)ctx;
    (void)info;
    (void)hmac;
    return 0;
}

static inline int mbedtls_md_hmac_starts(mbedtls_md_context_t* ctx, const unsigned char* key, size_t keylen) {
    if (!ctx) {
        return -1;
    }
    ctx->state = mbedtls_stub_mix(1469598103934665603ULL, key, keylen);
    return 0;
}

static inline int mbedtls_md_hmac_update(mbedtls_md_context_t* ctx, const unsigned char* input, size_t ilen) {
    if (!ctx) {
        return -1;
    }
    ctx->state = mbedtls_stub_mix(ctx->state, input, ilen);
    return 0;
}

static inline int mbedtls_md_hmac_finish(mbedtls_md_context_t* ctx, unsigned char* output) {
    if (!ctx || !output) {
        return -1;
    }
    uint64_t value = ctx->state;
    for (size_t i = 0; i < 32; ++i) {
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        output[i] = static_cast<unsigned char>((value * 2685821657736338717ULL) >> 56);
        value += 0x9E3779B97F4A7C15ULL + i;
    }
    return 0;
}

static inline void mbedtls_md_free(mbedtls_md_context_t* ctx) {
    if (ctx) {
        ctx->state = 0;
    }
}

#ifdef __cplusplus
}
#endif
