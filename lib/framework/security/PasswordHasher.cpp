#include <security/PasswordHasher.h>

#include "../../src/config/Network.h"

#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <array>
#include <cctype>
#include <cstring>
#include <cstdio>

namespace {

constexpr char kDelimiter = '$';

bool hasHashSchemePrefix(const String &storedCredential)
{
    const char *scheme = NET::AUTH::PASSWORD_HASH_SCHEME;
    const size_t schemeLen = strlen(scheme);
    return storedCredential.length() > schemeLen &&
           storedCredential.startsWith(scheme) &&
           storedCredential[schemeLen] == kDelimiter;
}

String bytesToHex(const uint8_t *data, size_t len)
{
    String hex;
    hex.reserve(len * 2);
    static constexpr char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++)
    {
        const uint8_t value = data[i];
        hex += kHexDigits[value >> 4];
        hex += kHexDigits[value & 0x0F];
    }
    return hex;
}

bool hexToBytes(const String &hex, uint8_t *out, size_t outLen)
{
    if (hex.length() != outLen * 2)
    {
        return false;
    }

    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < outLen; i++)
    {
        const int hi = hexValue(hex[i * 2]);
        const int lo = hexValue(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return true;
}

bool constantTimeEquals(const uint8_t *lhs, const uint8_t *rhs, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
    {
        diff |= lhs[i] ^ rhs[i];
    }
    return diff == 0;
}

bool deriveKey(const String &password,
               const uint8_t *salt,
               size_t saltLen,
               uint32_t iterations,
               uint8_t *output,
               size_t outputLen)
{
    return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                         reinterpret_cast<const unsigned char *>(password.c_str()),
                                         password.length(),
                                         salt,
                                         saltLen,
                                         iterations,
                                         static_cast<uint32_t>(outputLen),
                                         output) == 0;
}

bool parseHash(const String &storedHash,
               uint32_t *iterationsOut,
               std::array<uint8_t, NET::AUTH::PASSWORD_SALT_BYTES> *saltOut,
               std::array<uint8_t, NET::AUTH::PASSWORD_DERIVED_KEY_BYTES> *derivedKeyOut)
{
    if (!iterationsOut || !saltOut || !derivedKeyOut)
    {
        return false;
    }

    if (!hasHashSchemePrefix(storedHash))
    {
        return false;
    }

    const int firstDelimiter = storedHash.indexOf(kDelimiter);
    if (firstDelimiter < 0)
    {
        return false;
    }

    const int secondDelimiter = storedHash.indexOf(kDelimiter, firstDelimiter + 1);
    if (secondDelimiter < 0)
    {
        return false;
    }

    const int thirdDelimiter = storedHash.indexOf(kDelimiter, secondDelimiter + 1);
    if (thirdDelimiter < 0)
    {
        return false;
    }

    const String iterationsStr = storedHash.substring(firstDelimiter + 1, secondDelimiter);
    const String saltHex = storedHash.substring(secondDelimiter + 1, thirdDelimiter);
    const String keyHex = storedHash.substring(thirdDelimiter + 1);

    if (iterationsStr.isEmpty() || saltHex.isEmpty() || keyHex.isEmpty())
    {
        return false;
    }

    *iterationsOut = static_cast<uint32_t>(iterationsStr.toInt());
    if (*iterationsOut == 0)
    {
        return false;
    }

    return hexToBytes(saltHex, saltOut->data(), saltOut->size()) &&
           hexToBytes(keyHex, derivedKeyOut->data(), derivedKeyOut->size());
}

} // namespace

String PasswordHasher::hashPassword(const String &password)
{
    std::array<uint8_t, NET::AUTH::PASSWORD_SALT_BYTES> salt{};
    std::array<uint8_t, NET::AUTH::PASSWORD_DERIVED_KEY_BYTES> derivedKey{};

    esp_fill_random(salt.data(), salt.size());

    if (!deriveKey(password,
                   salt.data(),
                   salt.size(),
                   NET::AUTH::PASSWORD_HASH_ITERATIONS,
                   derivedKey.data(),
                   derivedKey.size()))
    {
        return String();
    }

    const String saltHex = bytesToHex(salt.data(), salt.size());
    const String keyHex = bytesToHex(derivedKey.data(), derivedKey.size());
    if (saltHex.isEmpty() || keyHex.isEmpty())
    {
        return String();
    }

    const String iterationString = String(static_cast<int>(NET::AUTH::PASSWORD_HASH_ITERATIONS));

    String encoded = String(NET::AUTH::PASSWORD_HASH_SCHEME);
    encoded.reserve(encoded.length() + 3 + iterationString.length() + saltHex.length() + keyHex.length());
    encoded += kDelimiter;
    encoded += iterationString;
    encoded += kDelimiter;
    encoded += saltHex;
    encoded += kDelimiter;
    encoded += keyHex;
    return encoded;
}

bool PasswordHasher::verifyPassword(const String &password, const String &storedHash)
{
    uint32_t iterations = 0;
    std::array<uint8_t, NET::AUTH::PASSWORD_SALT_BYTES> salt{};
    std::array<uint8_t, NET::AUTH::PASSWORD_DERIVED_KEY_BYTES> expected{};
    std::array<uint8_t, NET::AUTH::PASSWORD_DERIVED_KEY_BYTES> actual{};

    if (!parseHash(storedHash, &iterations, &salt, &expected))
    {
        return false;
    }

    if (!deriveKey(password, salt.data(), salt.size(), iterations, actual.data(), actual.size()))
    {
        return false;
    }

    return constantTimeEquals(actual.data(), expected.data(), expected.size());
}

bool PasswordHasher::matchesStoredCredential(const String &password, const String &storedCredential)
{
    if (isHashedCredential(storedCredential))
    {
        return verifyPassword(password, storedCredential);
    }

    return storedCredential == password;
}

bool PasswordHasher::isHashedCredential(const String &storedCredential)
{
    return hasHashSchemePrefix(storedCredential);
}
