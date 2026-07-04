#include <security/JwtTokenService.h>
#include <security/PasswordHasher.h>

#include "../../src/config/Network.h"
#include "../../src/system/logging/Logging.h"

#include <time.h>

#undef LOG_TAG
#define LOG_TAG "Security"

namespace {
constexpr const char* kRevocationClaim = "rev";

String toHexString(const uint8_t *data, size_t len)
{
    char hexBuffer[65] = {0};
    const size_t maxBytes = (sizeof(hexBuffer) - 1) / 2;
    const size_t bytesToEncode = len < maxBytes ? len : maxBytes;
    for (size_t i = 0; i < bytesToEncode; i++)
    {
        snprintf(&hexBuffer[i * 2], sizeof(hexBuffer) - (i * 2), "%02x", data[i]);
    }
    return String(hexBuffer);
}

String resolveJwtSecret(const String &configuredSecret)
{
    if (!configuredSecret.isEmpty() && configuredSecret != "random")
    {
        return configuredSecret;
    }

    static String volatileSecret;
    if (volatileSecret.isEmpty())
    {
        uint8_t randData[32];
        esp_fill_random(randData, sizeof(randData));
        volatileSecret = toHexString(randData, sizeof(randData));
        LOGI("Generated volatile JWT secret for this boot session");
    }

    return volatileSecret;
}

void populateJWTPayload(JsonObject &payload, const User *user)
{
    // Keep appliance sessions minimal and time-agnostic: identity/role are the
    // only stable claims we need on-device.
    payload["username"] = user->username;
    payload["admin"] = user->admin;
    payload[kRevocationClaim] = static_cast<long>(user->validAfter);
}
} // namespace

JwtTokenService::JwtTokenService(const String &factorySecret)
    : _jwtHandler(factorySecret)
{
}

void JwtTokenService::configureSecret(const String &configuredSecret)
{
    _jwtHandler.setSecret(resolveJwtSecret(configuredSecret));
}

Authentication JwtTokenService::authenticateJWT(const char *jwt, const std::list<User> &users)
{
    if (!jwt)
    {
        return Authentication();
    }

    JsonDocument payloadDocument;
    _jwtHandler.parseJWT(String(jwt), payloadDocument);
    if (!payloadDocument.is<JsonObject>())
    {
        return Authentication();
    }

    JsonObject parsedPayload = payloadDocument.as<JsonObject>();

    String username = parsedPayload["username"] | "";
    for (const auto &user : users)
    {
        if (user.username == username && validatePayload(parsedPayload, &user))
        {
            return Authentication(user);
        }
    }

    return Authentication();
}

Authentication JwtTokenService::authenticateCredentials(const String &username,
                                                        const String &password,
                                                        const std::list<User> &users) const
{
    for (const auto &user : users)
    {
        if (user.username == username && PasswordHasher::matchesStoredCredential(password, user.password))
        {
            return Authentication(user);
        }
    }

    return Authentication();
}

String JwtTokenService::generateJWT(User *user)
{
    if (!user)
    {
        return String();
    }

    JsonDocument jsonDocument;
    JsonObject payload = jsonDocument.to<JsonObject>();
    populateJWTPayload(payload, user);

    // iat is still useful for explicit revocation via validAfter, but the token
    // no longer carries exp or boot-session-only semantics.
    payload["iat"] = time(nullptr);

    return _jwtHandler.buildJWT(payload);
}

boolean JwtTokenService::validatePayload(JsonObject &parsedPayload, const User *user) const
{
    if (!user)
    {
        return false;
    }

    if (parsedPayload["username"] != user->username)
    {
        return false;
    }
    if (parsedPayload["admin"] != user->admin)
    {
        return false;
    }

    if (parsedPayload[kRevocationClaim].is<long>())
    {
        const long tokenRevocationStamp = parsedPayload[kRevocationClaim];
        const long currentRevocationStamp = static_cast<long>(user->validAfter);
        if (tokenRevocationStamp != currentRevocationStamp)
        {
            LOGW("Token rejected: revocation stamp mismatch (token: %ld, current: %ld)",
                 tokenRevocationStamp,
                 currentRevocationStamp);
            return false;
        }
    }

    if (parsedPayload["iat"].is<time_t>())
    {
        // validAfter remains the single revocation control after removing exp.
        time_t iat = parsedPayload["iat"];
        if (iat < user->validAfter)
        {
            LOGW("Token rejected: revoked (iat: %ld, validAfter: %ld)",
                 (long)iat,
                 (long)user->validAfter);
            return false;
        }
    }

    return true;
}
