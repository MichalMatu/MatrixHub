#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/md.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int mbedtls_pkcs5_pbkdf2_hmac_ext(mbedtls_md_type_t md_type,
                                                const unsigned char *password,
                                                size_t plen,
                                                const unsigned char *salt,
                                                size_t slen,
                                                unsigned int iteration_count,
                                                uint32_t key_length,
                                                unsigned char *output)
{
    if (md_type != MBEDTLS_MD_SHA256 || !output) {
        return -1;
    }

    uint32_t state = 0x9e3779b9u ^ iteration_count ^ key_length;
    for (size_t i = 0; i < plen; ++i) {
        state ^= static_cast<uint32_t>(password[i]) + 0x7f4a7c15u + (state << 6) + (state >> 2);
    }
    for (size_t i = 0; i < slen; ++i) {
        state ^= static_cast<uint32_t>(salt[i]) + 0x85ebca6bu + (state << 6) + (state >> 2);
    }

    for (uint32_t i = 0; i < key_length; ++i) {
        state ^= (state << 13);
        state ^= (state >> 17);
        state ^= (state << 5);
        state += (iteration_count * 33u) + i;
        output[i] = static_cast<unsigned char>((state >> ((i % 4) * 8)) & 0xFFu);
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
